/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing implementation
 ******************************************************************************/
#include <stdbool.h>
#include <string.h>
#include "sl_common.h"
#include "em_system.h"

#include "ecode.h"
#include "nvm3.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/oid.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include "psa/crypto_values.h"

#include "sl_bt_cbap_root_cert.h"
#include "sl_cbap+_passkey.h"

#include "app_log.h"
// -----------------------------------------------------------------------------
// Defines

#define NVM3_HANDLE                   (nvm3_defaultHandle)
#define CHAIN_LINK_DATA_LEN           192                    // Length of an NVM3 chunk
#define CHAIN_LINK_DATA_NUM           4                      // Number of how many chunks needed for a certificate
#define NUMBER_OF_CERTIFIACTE_RECORDS 3                      // Number of certificate records

#define POS_DEVICE_CERTIFICATE        0                      // Device certificate position in the record control block
#define POS_DEVICE_EC_KEY             1                      // EC Key pair position in the record control block

#define CBAP_NVM3_REGION              0x40000                // NVM3 region used for CBAP
#define CONTROL_BLOCK_KEY             0x0400                 // Next control block NVM3 ID (points to itself if last)
#define DEVICE_EC_KEY_PAIR_ID         (CBAP_NVM3_REGION | 0x00FE)

#define SIGNATURE_LEN                 64                     // ECDSA signature length (P-256)
#define CERT_IND_CHUNK_LEN            100
#define EC_PUB_KEY_LEN                65
#define PUB_KEY_OFFSET                26

#define PASSKEY_LEN                   4                      // PassKey is 6-digit number (stored as uint32_t)
#define SIGNATURE_DATA_LEN            (PASSKEY_LEN + SIGNATURE_LEN)

// -----------------------------------------------------------------------------
// Type definitions

// Certificate record control block
typedef struct __attribute__((__packed__)) control_block_s {
  uint16_t header;
  uint16_t next;
  uint64_t bitmap;
  uint8_t nvm3_key[NUMBER_OF_CERTIFIACTE_RECORDS];
  uint16_t max_link_data_len;
} control_block_t;

typedef struct certificate_chunk_s {
  uint16_t header;
  uint16_t entry_id;
  uint16_t data_lenght;
  uint8_t data[CHAIN_LINK_DATA_LEN];
} certificate_chunk_t;

// -----------------------------------------------------------------------------
// Module variables

// Root certificate in PEM format
const char *cbap_root_certificate_pem = SL_BT_CBAP_ROOT_CERT;
// MBEDTLS certificate contexts
static mbedtls_x509_crt root_certificate_context;

// Public key ID of the remote device
static mbedtls_svc_key_id_t remote_pub_key_id = 0;

// -----------------------------------------------------------------------------
// Private function declarations

// Read the certificate record control block from NVM3.
static sl_status_t get_control_block(control_block_t *control_block);
// Read data chunks from NVM3 and reconstruct the certificate.
static sl_status_t get_certificate(uint8_t *cert, uint32_t *cert_len, uint32_t nvm3_key);

// Read data from NVM3 located at a specified NVM3 key.
static sl_status_t get_nvm3_object(uint32_t key, uint8_t *buf, size_t *len, size_t maxlen);

// Converts NVM3 status codes to SL status codes.
static sl_status_t nvm3_status_to_sl_status(Ecode_t sc);
// Converts PSA status code to SL status code.
static sl_status_t psa_status_to_sl_status(psa_status_t sc);

// -----------------------------------------------------------------------------
// Public function definitions

/******************************************************************************
 * Imports and validates the device with root certificate.
 *
 * @param[out] device_certificate_der device certificate in DER format.
 * @param[out] device_certificate_der_len device certificate length.
 *
 * @return SL_STATUS_OK if device certificate is validated, error code otherwise.
 *****************************************************************************/
sl_status_t sl_cbap_init(uint8_t *device_certificate_der, uint32_t *device_certificate_der_len)
{
  sl_status_t sc;
  int mbedtls_ret = 0;
  mbedtls_x509_crt dev_certificate_context;

  if (cbap_root_certificate_pem == NULL || device_certificate_der == NULL || device_certificate_der_len == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  SYSTEM_SecurityCapability_TypeDef capability;
  capability = SYSTEM_GetSecurityCapability();
  if (capability != securityCapabilityRoT
      && capability != securityCapabilitySE
      && capability != securityCapabilityVault) {
    return SL_STATUS_NOT_SUPPORTED;
  }

  sc = psa_status_to_sl_status(psa_crypto_init());
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Get control block
  control_block_t control_block;
  sc = get_control_block(&control_block);
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Get device certificate
  uint32_t dev_cert_nvm3_key = CBAP_NVM3_REGION | CONTROL_BLOCK_KEY | control_block.nvm3_key[POS_DEVICE_CERTIFICATE];
  sc = get_certificate(device_certificate_der, device_certificate_der_len, dev_cert_nvm3_key);

  if (sc != SL_STATUS_OK) {
    return sc;
  }

  mbedtls_x509_crt_init(&dev_certificate_context);
  // Parse/check format PEM or DER
  mbedtls_ret = mbedtls_x509_crt_parse(&dev_certificate_context,
                                       (const unsigned char *)device_certificate_der,
                                       *device_certificate_der_len);
  if (mbedtls_ret != 0) {
    return SL_STATUS_FAIL;
  }

  mbedtls_x509_name *tmp = &dev_certificate_context.issuer;
  app_log_info("--------------ISSUER----------------" APP_LOG_NL);
  while(tmp != NULL)
  {
    // Examp: OID = 55 04 0a -> X.520 = 2.5.4.10 (id-at-organizationName)
    // Organization Name
    app_log("OID (hex): ");
    for(size_t i = 0; i < tmp->oid.len; i++)
    {
      app_log("%02x ", tmp->oid.p[i]);
    }
    app_log("\nName (len = %u): %.*s" APP_LOG_NL, 
            tmp->val.len, (int)tmp->val.len, tmp->val.p);
    
    tmp = tmp->next;
  }
  app_log_info("------------------------------------" APP_LOG_NL);

  mbedtls_x509_name *tmp2 = &dev_certificate_context.subject;
  app_log_info("--------------SUBJECT----------------" APP_LOG_NL);
  while(tmp2 != NULL)
  {
    // Examp: OID = 55 04 0a -> X.520 = 2.5.4.10 (id-at-organizationName)
    // Organization Name
    app_log("OID (hex): ");
    for(size_t i = 0; i < tmp2->oid.len; i++)
    {
      app_log("%02x ", tmp2->oid.p[i]);
    }
    app_log("\nName (len = %u): %.*s" APP_LOG_NL, 
            tmp2->val.len, (int)tmp2->val.len, tmp2->val.p);
    
    tmp2 = tmp2->next;
  }
  app_log_info("------------------------------------" APP_LOG_NL);

  mbedtls_x509_buf *raw_pk = &dev_certificate_context.pk_raw;
  app_log_info("------------PUBLIC KEY--------------" APP_LOG_NL);
  for(size_t i = 0; i < raw_pk->len; i++)
  {
    app_log("%02x ", raw_pk->p[i]);
  }
  app_log_info("------------------------------------" APP_LOG_NL);

  // Get root certificate
  if (strlen(cbap_root_certificate_pem) == 0) {
    return SL_STATUS_FAIL;
  }
  app_log_info("Root Cert:\n%s" APP_LOG_NL, cbap_root_certificate_pem);

  mbedtls_x509_crt_init(&root_certificate_context);

  uint8_t root_certificate_der[CHAIN_LINK_DATA_LEN * CHAIN_LINK_DATA_NUM] = { 0 };
  size_t root_certificate_der_len;
  mbedtls_ret = mbedtls_base64_decode(root_certificate_der,
                                      sizeof(root_certificate_der),
                                      &root_certificate_der_len,
                                      (const unsigned char *)cbap_root_certificate_pem,
                                      strlen(cbap_root_certificate_pem));
  if (mbedtls_ret != 0) {
    return SL_STATUS_FAIL;
  }
  app_log_info("Decode completely" APP_LOG_NL);

  mbedtls_ret = mbedtls_x509_crt_parse(&root_certificate_context,
                                       (const unsigned char *)root_certificate_der,
                                       root_certificate_der_len);
  if (mbedtls_ret != 0) {
    return SL_STATUS_FAIL;
  }

  // uint32_t flags;
  // mbedtls_ret = mbedtls_x509_crt_verify(&root_certificate_context,
  //                                       &root_certificate_context,
  //                                       NULL,
  //                                       NULL,
  //                                       &flags,
  //                                       NULL,
  //                                       NULL);
  // if (mbedtls_ret != 0) {
  //   return SL_STATUS_FAIL;
  // }

  // mbedtls_ret = x509_crt_check_signature((const mbedtls_x509_crt *) &root_certificate_context,
  //                                         &root_certificate_context,
  //                                         NULL);
  // if (mbedtls_ret != 0) {
  //   app_log_info("ERROR signature checking" APP_LOG_NL);
  //   return SL_STATUS_FAIL;
  // }

  uint32_t flags;
  // Validate device certificate with the root certificate
  mbedtls_ret = mbedtls_x509_crt_verify(&dev_certificate_context,
                                        &root_certificate_context,
                                        NULL,
                                        NULL,
                                        &flags,
                                        NULL,
                                        NULL);
  (void)flags;
  if (mbedtls_ret != 0) {
      app_log_info("ERROR verifying remote certificate %d" APP_LOG_NL, (int)mbedtls_ret);
    return SL_STATUS_FAIL;
  }

  app_log_info("Verifying remote certificate %d" APP_LOG_NL, (int)mbedtls_ret);

  // Check the presence of the EC key pair.
  if ((control_block.bitmap & (1 << POS_DEVICE_EC_KEY)) == 0) {
    return SL_STATUS_FAIL;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Parse and validate remote certificate and extract remote public key.
 *
 * @param[in] remote_certificate_der Certificate from remote device in DER.
 * @param[in] remote_certificate_der_len Length of the remote certificate.
 *
 * @return SL_STATUS_OK if remote certificate is verified, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_process_remote_cert(uint8_t *remote_certificate_der, uint32_t remote_certificate_der_len)
{
  sl_status_t sc;
  int mbedtls_ret = 0;
  mbedtls_x509_crt remote_certificate_context;

  if (remote_certificate_der == NULL || remote_certificate_der_len == 0) {
    return SL_STATUS_NULL_POINTER;
  }

  // Initialize and parse remote certificate
  mbedtls_x509_crt_init(&remote_certificate_context);
  mbedtls_ret = mbedtls_x509_crt_parse(&remote_certificate_context,
                                       (const unsigned char *)remote_certificate_der,
                                       remote_certificate_der_len);
  if (mbedtls_ret != 0) {
    return SL_STATUS_FAIL;
  }

  unsigned char buf[1024];
  size_t olen;

  // Log
  mbedtls_ret = mbedtls_base64_encode(buf,
                                      sizeof(buf),
                                      &olen,
                                      remote_certificate_context.raw.p,
                                      remote_certificate_context.raw.len);
  (void)olen;
  if (mbedtls_ret != 0) {
    return SL_STATUS_FAIL;
  }

  // Validate it with the root certificate
  uint32_t flags;
  mbedtls_ret = mbedtls_x509_crt_verify(&remote_certificate_context,
                                        &root_certificate_context,
                                        NULL,
                                        NULL,
                                        &flags,
                                        NULL,
                                        NULL);
  (void)flags;
  if (mbedtls_ret != 0) {
    app_log_info("ERROR verifying remote certificate %d" APP_LOG_NL, (int)mbedtls_ret);
    return SL_STATUS_FAIL;
  }
  app_log_info("Verifying remote certificate %d" APP_LOG_NL, (int)mbedtls_ret);

  // Get the public key from the remote certificate and set attributes
  // Store Public key from remote device to PSA storage (ITS flash??) with Psa pk ID
  psa_key_attributes_t remote_pub_key_attr = PSA_KEY_ATTRIBUTES_INIT;
  psa_set_key_algorithm(&remote_pub_key_attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
  psa_set_key_type(&remote_pub_key_attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_usage_flags(&remote_pub_key_attr, PSA_KEY_USAGE_VERIFY_MESSAGE);

  sc = psa_status_to_sl_status(psa_import_key(&remote_pub_key_attr,
                                              &remote_certificate_context.pk_raw.p[PUB_KEY_OFFSET],
                                              EC_PUB_KEY_LEN,
                                              &remote_pub_key_id));

  if (sc != SL_STATUS_OK) {
    app_log("Error 1 %04lx\n", sc);
    return sc;
  }

  app_log("Import- ID %08lx" APP_LOG_NL, remote_pub_key_id);

  mbedtls_x509_crt_free(&remote_certificate_context);

  return SL_STATUS_OK;
}

/*******************************************************************************
 * @brief Signs the passkey using ECDSA.
 *
 * @param[in] passkey The 6-digit passkey value (0-999999)
 * @param[out] output_data Buffer for signed passkey data (must be >= 68 bytes)
 * @param[out] output_len The signed passkey data length
 *
 * @return SL_STATUS_OK if OOB data signed, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_sign_passkey(uint32_t passkey,
                                 uint8_t *output_data,
                                 size_t *output_len)
{
  sl_status_t sc;
  uint8_t passkey_bytes[PASSKEY_LEN];
  uint8_t signature[SIGNATURE_LEN];
  size_t signature_length;

  if (passkey > 999999)
  {
    return SL_STATUS_INVALID_KEY;
  }
  
  if (output_data == NULL || output_len == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  // Convert passkey to byte array (little-endian)
  passkey_bytes[0] = (uint8_t)(passkey & 0xFF);
  passkey_bytes[1] = (uint8_t)((passkey >> 8) & 0xFF);
  passkey_bytes[2] = (uint8_t)((passkey >> 16) & 0xFF);
  passkey_bytes[3] = (uint8_t)((passkey >> 24) & 0xFF);

  // Sign the OOB
  sc = psa_status_to_sl_status(psa_sign_message(DEVICE_EC_KEY_PAIR_ID,
                                                PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                                passkey_bytes,
                                                sizeof(passkey_bytes),
                                                signature,
                                                sizeof(signature),
                                                &signature_length));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  memcpy(output_data, passkey_bytes, PASSKEY_LEN);
  memcpy(&output_data[PASSKEY_LEN], signature, signature_length);

  *output_len = SIGNATURE_DATA_LEN;
  return SL_STATUS_OK;
}

/*******************************************************************************
 * Verifies the signed passkey using ECDSA.
 *
 * @param[in] passkey_bytes Passkey stored in array comes from remote device. 
 * @param[in] remote_passkey_signature Remote Passkey's signature.
 *
 * @return SL_STATUS_OK if data signature is OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_verify_signed_passkey(uint8_t *passkey_bytes,
                                          uint8_t *remote_passkey_signature)
{
  uint8_t passkey_data[PASSKEY_LEN];
  sl_status_t sc;

  if (passkey_bytes == NULL || remote_passkey_signature == NULL) {
    return SL_STATUS_NULL_POINTER;
  }

  memcpy(passkey_data, passkey_bytes, PASSKEY_LEN);

  sc = psa_status_to_sl_status(psa_verify_message(remote_pub_key_id,
                                                  PSA_ALG_ECDSA(PSA_ALG_SHA_256),
                                                  passkey_data,
                                                  sizeof(passkey_data),
                                                  remote_passkey_signature,
                                                  SIGNATURE_LEN));
  if (sc != SL_STATUS_OK) {
    return sc;
  }

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Destroys the keys which were used during the CBAP process.
 *
 * @return SL_STATUS_OK if OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_destroy_key(void)
{
  // remote_pub_key_id
  return psa_status_to_sl_status(psa_destroy_key(remote_pub_key_id));
}

// -----------------------------------------------------------------------------
// Private function definitions

/*******************************************************************************
 * Read the certificate record control block from NVM3.
 *
 * @param[out] control_block pointer to the control block structure
 * @return SL_STATUS_OK - if successful, error code otherwise.
 ******************************************************************************/
static sl_status_t get_control_block(control_block_t *control_block)
{
  sl_status_t sc;
  size_t nvm3_obj_len;

  sc = get_nvm3_object(CBAP_NVM3_REGION | CONTROL_BLOCK_KEY,
                       (uint8_t *)control_block,
                       &nvm3_obj_len,
                       sizeof(control_block_t));
  (void)nvm3_obj_len;
  return sc;
}

/*******************************************************************************
 * Read data chunks from NVM3 and reconstruct the certificate.
 *
 * @param[out] cert Certificate buffer.
 * @param[out] cert_len Certificate length.
 * @param[in]  nvm3_key the NVM3 key of the first certificate chunk.
 ******************************************************************************/
static sl_status_t get_certificate(uint8_t *cert, uint32_t *cert_len, uint32_t nvm3_key)
{
  sl_status_t sc;
  certificate_chunk_t certificate_chunk;
  size_t nvm3_obj_len;
  uint8_t next_chuck_exist = 0;
  *cert_len = 0;
  int i = 0;

  do {
    sc = get_nvm3_object(nvm3_key, (uint8_t *)&certificate_chunk, &nvm3_obj_len, sizeof(certificate_chunk));
    (void)nvm3_obj_len;
    if (sc != SL_STATUS_OK) {
      return sc;
    }

    memcpy(&cert[i * CHAIN_LINK_DATA_LEN], certificate_chunk.data, CHAIN_LINK_DATA_LEN);
    next_chuck_exist = (certificate_chunk.header & (1 << 1));
    *cert_len += certificate_chunk.data_lenght;

    nvm3_key++;
    i++;
  } while (next_chuck_exist != 0);

  return SL_STATUS_OK;
}

/*******************************************************************************
 * Read data from NVM3 located at a specified NVM3 key.
 *
 * @param[in] key the key of the NVM3 object.
 * @param[out] buf buffer to write.
 * @param[out] len the size of the NVM3 object.
 * @param[in] maxlen maximum size to read.
 * @return SL_STATUS_OK - if successful, error code otherwise.
 ******************************************************************************/
static sl_status_t get_nvm3_object(uint32_t key, uint8_t *buf, size_t *len, size_t maxlen)
{
  sl_status_t sc;
  uint32_t type;
  size_t read_len;

  // Clamp read size to maxlen
  sc = nvm3_status_to_sl_status(nvm3_getObjectInfo(NVM3_HANDLE, key, &type, len));
  (void)type;

  if (sc != SL_STATUS_OK) {
    return sc;
  }

  // Read NVM3 data
  read_len = (*len > maxlen) ? maxlen : *len;
  sc = nvm3_status_to_sl_status(nvm3_readData(NVM3_HANDLE, key, buf, read_len));
  return sc;
}

/*******************************************************************************
 * Converts NVM3 status codes to SL status codes.
 *
 * @param[in] sc NVM3 status code
 * @return SL status code.
 ******************************************************************************/
static sl_status_t nvm3_status_to_sl_status(Ecode_t sc)
{
  switch (sc) {
    case ECODE_OK:                      return SL_STATUS_OK;
    case ECODE_NVM3_ERR_PARAMETER:      return SL_STATUS_INVALID_PARAMETER;
    case ECODE_NVM3_ERR_KEY_INVALID:    return SL_STATUS_INVALID_PARAMETER;
    case ECODE_NVM3_ERR_KEY_NOT_FOUND:  return SL_STATUS_BT_PS_KEY_NOT_FOUND;
    case ECODE_NVM3_ERR_STORAGE_FULL:   return SL_STATUS_BT_PS_STORE_FULL;
    default:                            return (sl_status_t) (SL_STATUS_BLUETOOTH_SPACE + 0x80 + (sc & (~ECODE_EMDRV_NVM3_BASE)));
  }
}

/*******************************************************************************
 * Converts PSA status code to SL status code.
 *
 * @param[in] sc PSA status code
 * @return SL status code.
 ******************************************************************************/
static sl_status_t psa_status_to_sl_status(psa_status_t sc)
{
  switch (sc) {
    case PSA_SUCCESS:                     return SL_STATUS_OK;
    case PSA_ERROR_GENERIC_ERROR:         return SL_STATUS_FAIL;
    case PSA_ERROR_NOT_SUPPORTED:         return SL_STATUS_NOT_SUPPORTED;
    case PSA_ERROR_NOT_PERMITTED:         return SL_STATUS_PERMISSION;
    case PSA_ERROR_BUFFER_TOO_SMALL:      return SL_STATUS_WOULD_OVERFLOW;
    case PSA_ERROR_ALREADY_EXISTS:        return SL_STATUS_ALREADY_EXISTS;
    case PSA_ERROR_DOES_NOT_EXIST:        return SL_STATUS_FAIL;
    case PSA_ERROR_BAD_STATE:             return SL_STATUS_INVALID_STATE;
    case PSA_ERROR_INVALID_ARGUMENT:      return SL_STATUS_INVALID_PARAMETER;
    case PSA_ERROR_INSUFFICIENT_MEMORY:   return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_INSUFFICIENT_STORAGE:  return SL_STATUS_NO_MORE_RESOURCE;
    case PSA_ERROR_COMMUNICATION_FAILURE: return SL_STATUS_IO;
    case PSA_ERROR_STORAGE_FAILURE:       return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_HARDWARE_FAILURE:      return SL_STATUS_BT_HARDWARE;
    case PSA_ERROR_CORRUPTION_DETECTED:   return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_INSUFFICIENT_ENTROPY:  return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_SIGNATURE:     return SL_STATUS_INVALID_SIGNATURE;
    case PSA_ERROR_INVALID_PADDING:       return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INSUFFICIENT_DATA:     return SL_STATUS_BT_CRYPTO;
    case PSA_ERROR_INVALID_HANDLE:        return SL_STATUS_INVALID_HANDLE;
    case PSA_ERROR_DATA_CORRUPT:          return SL_STATUS_BT_DATA_CORRUPTED;
    case PSA_ERROR_DATA_INVALID:          return SL_STATUS_BT_CRYPTO;
    default:                              return SL_STATUS_BT_UNSPECIFIED;
  }
}
