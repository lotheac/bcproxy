#ifndef buffer_h
#define buffer_h

typedef struct buffer buffer_t;

struct buffer {
    char   *data;
    size_t len;
    size_t sz;
};

buffer_t *buffer_new(size_t initial_size);
void buffer_free(buffer_t *buffer);
int buffer_append(buffer_t *buf, const char *input, size_t len);
int buffer_append_buf(buffer_t *buf, const buffer_t *ibuf);
int buffer_append_str(buffer_t *buf, const char *str);
void buffer_clear(buffer_t *buf);

#endif /* buffer_h */
