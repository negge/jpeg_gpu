#if !defined(_internal_H)
# define _internal_H (1)

# define GLJ_MAXI(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))
# define GLJ_MINI(a, b) ((a) ^ (((b) ^ (a)) & -((b) < (a))))

# define GLJ_ILOG(x) (glj_ilog(x))

int glj_ilog(unsigned int _v);

/*Clamps a signed integer between 0 and 255, returning an unsigned char.
 *   This assumes a char is 8 bits.*/
#define GLJ_CLAMP255(x) \
 ((unsigned char)((((x) < 0) - 1) & ((x) | -((x) > 255))))

void *glj_aligned_malloc(size_t _sz,size_t _align);
void glj_aligned_free(void *_ptr);

#endif
