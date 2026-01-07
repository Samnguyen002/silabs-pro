#include "app_iostream_usart.h"

#ifndef BUFSIZE
#define BUFSIZE 80
#endif

// Define init function
void app_iostream_usart_init(void)
{
  // Prevent buffering of output/input.
  // close the stdout and stdin buffering and use our instance of USART for I/O
#if !defined(__CROSSWORKS_ARM) && defined(__GNUC__)
  setvbuf(stdout, NULL, _IONBF, 0);   // Set unbuffered mode for stdout (newlib)
  setvbuf(stdin, NULL, _IONBF, 0);    // Set unbuffered mode for stdin (newlib)
#endif

  // Output on vcom usart instance
  // my instance name is sl_iostream_exp
  const char str1[] = "USART I/O stream start initializing...\r\n\r\n";
  sl_iostream_write(sl_iostream_vcom_handle, str1, strlen(str1));

  // setting default stream
  sl_iostream_set_default(sl_iostream_vcom_handle);
  const char str2[] = "This is output on default stream\r\n";
  sl_iostream_write(SL_IOSTREAM_STDOUT, str2, strlen(str2));

  // Now both stdout and stdin are mapped to sl_iostream_exp_handle
  // Using printf
  // need intal IO Stream component and retarget-stdio component if not using printf
  printf("Printf uses the default stream, as long as iostream_retarget_stdio included\r\n");
}

uint8_t app_iostream_checksum(uint8_t *data, uint8_t data_len)
{
  uint32_t sum = 0U;
  uint8_t checksum;
  for(int i = 0; i < data_len; i++)
  {
    sum += data[i];
  }

  checksum = sum & 0xFF;
  return ~checksum + 1;
}
