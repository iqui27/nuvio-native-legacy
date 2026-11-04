// Host stand-in for the bionic header; the test defines the function.
#define PROP_VALUE_MAX 92
int __system_property_get(const char *name, char *value);
