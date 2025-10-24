#define BUCKETS_PER_SEG 10
#define MAX_CUCKOO_KICK 8

#define BITS_PER_TAG 14
#define FINGUREPRINT_MASK (0x3FFFULL)

#define NUM_SEG_BITS (num_table_bits_ - BUCKETS_PER_SEG)
#define ACTV_TAG_BIT (num_table_bits_ - INIT_TABLE_BITS)

#define isn(x, bit) (x & (~(((x & (1 << bit)) >> bit) - 1)))
#define isl(x, bit) (x & ((((x & (1 << bit)) >> bit) - 1)))

#define ll_isl(x, bit) (x & (~(((x & (0x001001001001ULL << bit)) >> bit) * 0xFFFULL)))
#define ll_isn(x, bit) (x & ((((x & (0x001001001001ULL << bit)) >> bit) * 0xFFFULL)))
