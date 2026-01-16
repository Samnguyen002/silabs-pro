/***************************************************************************//**
 * @file
 * @brief Certificate Based Authentication and Pairing header
 *******************************************************************************
 * Customs from sl_bt_cbap for signing passkey
 ******************************************************************************/
#ifndef SL_CBAP_PASSKEY_H
#define SL_CBAP_PASSKEY_H

#include <stdint.h>
#include "sl_status.h"

/**************************************************************************//**
 * Imports and validates the device with root certificate.
 *
 * @param[out] device_certificate_der device certificate in DER format.
 * @param[out] device_certificate_der_len device certificate length.
 *
 * @return SL_STATUS_OK if device certificate is validated, error code otherwise.
 *****************************************************************************/
sl_status_t sl_cbap_init(uint8_t *device_certificate_der, uint32_t *device_certificate_der_len);

/***************************************************************************//**
 * Parse and validate remote certificate and extract remote public key.
 *
 * @param[in] remote_certificate_der Certificate from remote device in DER.
 * @param[in] remote_certificate_der_len Length of the remote certificate.
 *
 * @return SL_STATUS_OK if remote certificate is verified, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_process_remote_cert(uint8_t *remote_certificate_der, uint32_t remote_certificate_der_len);

/***************************************************************************//**
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
                                 size_t *output_len);

/***************************************************************************//**
 * Verifies the signed passkey using ECDSA.
 *
 * @param[in] passkey_bytes Passkey stored in array comes from remote device. 
 * @param[in] remote_passkey_signature Remote Passkey's signature.
 *
 * @return SL_STATUS_OK if data signature is OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_verify_signed_passkey(uint8_t *passkey_bytes,
                                          uint8_t *remote_passkey_signature);

/***************************************************************************//**
 * Destroys the keys which were used during the CBAP process.
 *
 * @return SL_STATUS_OK if OK, error code otherwise.
 ******************************************************************************/
sl_status_t sl_cbap_destroy_key(void);

#endif // SL_BT_CBAP_H
