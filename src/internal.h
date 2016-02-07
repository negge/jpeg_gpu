#if !defined(_internal_H)
# define _internal_H (1)

# define OD_MAXI(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))
# define OD_MINI(a, b) ((a) ^ (((b) ^ (a)) & -((b) < (a))))

# define OD_ILOG(x) (od_ilog(x))

int od_ilog(unsigned int _v);

#endif
