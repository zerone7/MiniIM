
#ifdef DEBUG_FRIEND
#define frd_dbg(format, arg...)         \
    printf("FRIEND: " format, ##arg);
#else
#define frd_dbg(format, arg...)         \
({                                      \
    if(0)                               \
        printf("FRIEND: " format, ##arg); \
    0;                                  \
})
#endif
