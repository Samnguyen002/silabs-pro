/***************************************************************************//**
 * @file
 * @brief Root certificate definition in PEM format.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#ifndef SL_BT_CBAP_ROOT_CERT_H
#define SL_BT_CBAP_ROOT_CERT_H

// Defining when root cert overrided by user
#define SLI_SUPPRESS_MISSING_CONFIGURATION_WARNING
#if !defined(SLI_SUPPRESS_MISSING_CONFIGURATION_WARNING)
#warning "Please insert the root certificate created with the certificate generator python script." \
  "For more details see the readme of the \"SoC - Certificate Signing Request Generator\" example."
#endif

// Root certificate used in sl_bt_cbap.c, functions cbap_init() or cbap_process_remote_cert()
// will be called with this cert for verifying Device cert
#define SL_BT_CBAP_ROOT_CERT \
  "MIICXjCCAgSgAwIBAgIURQyUQZUKsxRuAUFeQsjbGHn8nlEwCgYIKoZIzj0EAwIw" \
  "gZQxCzAJBgNVBAYTAlVTMR0wGwYDVQQKDBRTaWxpY29uIExhYm9yYXRvcmllczER" \
  "MA8GA1UECwwIV2lyZWxlc3MxDjAMBgNVBAgMBVRleGFzMQ8wDQYDVQQDDAZTaWxh" \
  "YnMxDzANBgNVBAcMBkF1c3RpbjEhMB8GCSqGSIb3DQEJARYSc3VwcG9ydEBzaWxh" \
  "YnMuY29tMB4XDTI1MTIwOTEwMjI1OVoXDTI2MTIwOTEwMjI1OVowgZQxCzAJBgNV" \
  "BAYTAlVTMR0wGwYDVQQKDBRTaWxpY29uIExhYm9yYXRvcmllczERMA8GA1UECwwI" \
  "V2lyZWxlc3MxDjAMBgNVBAgMBVRleGFzMQ8wDQYDVQQDDAZTaWxhYnMxDzANBgNV" \
  "BAcMBkF1c3RpbjEhMB8GCSqGSIb3DQEJARYSc3VwcG9ydEBzaWxhYnMuY29tMFkw" \
  "EwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEtNx3tKR0XafyVWg4gZgPZmJWoj7vb2yv" \
  "ge3Lw8FH1FyS/UVeFD3rrQUoPp5kmTOgt2OUdBDSqcqf3hJdWRQSQKMyMDAwHQYD" \
  "VR0OBBYEFJshZsnffME4ROvh0HEzXhbDFrAvMA8GA1UdEwEB/wQFMAMBAf8wCgYI" \
  "KoZIzj0EAwIDSAAwRQIgXwW4SGwabT1aXpIwc3K/LRHyR73qNhEOaVA+H7nxveQC" \
  "IQCjdhp3r449UcncZFwnAfwwlbypt8wgW05FTmp0+bk4WQ==" \

#endif // SL_BT_CBAP_ROOT_CERT_H
