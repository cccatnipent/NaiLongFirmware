#pragma once
#include <stddef.h>
// Image encoding is not exercised by the host MCP text/deferred-call tests.
int mbedtls_base64_encode(unsigned char*, size_t, size_t*, const unsigned char*, size_t);
