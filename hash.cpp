#include <cutils/hashmap.h>

int str_hash(void *key) {
    return hashmapHash(key, strlen((char*)key));
}

bool str_equals(void *key_a, void *key_b) {
    return strcmp((char*)key_a, (char*)key_b) == 0;
}
