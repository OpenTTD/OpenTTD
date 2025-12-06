add_files(
    freetypefontcache.cpp
    CONDITION Freetype_FOUND
)

add_files(
    iconfontcache.cpp
    iconfontcache.h
    spritefontcache.cpp
    spritefontcache.h
    truetypefontcache.cpp
    truetypefontcache.h
)
