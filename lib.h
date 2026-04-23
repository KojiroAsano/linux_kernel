#ifndef LIB_H_
#define LIB_H_

void memset(void *buffer, char value, int size);
void memmove(void *dest, const void *src, int size);
void memcpy(void *dest, const void *src, int size);
int memcmp(const void *s1, const void *s2, int size);

#endif