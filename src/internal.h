#if !defined(_internal_H)
# define _internal_H (1)

# define OD_MAXI(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))
# define OD_MINI(a, b) ((a) ^ (((b) ^ (a)) & -((b) < (a))))

# define OD_ILOG(x) (od_ilog(x))

int od_ilog(unsigned int _v);

/*Clamps a signed integer between 0 and 255, returning an unsigned char.
 *   This assumes a char is 8 bits.*/
#define OD_CLAMP255(x) \
 ((unsigned char)((((x) < 0) - 1) & ((x) | -((x) > 255))))

void *od_aligned_malloc(size_t _sz,size_t _align);
void od_aligned_free(void *_ptr);

#endif
