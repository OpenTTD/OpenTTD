add_files(
    freetypefontcache.cpp
    CONDITION Freetype_FOUND
)

add_files(
    spritefontcache.cpp
    spritefontcache.h
    truetypefontcache.cpp
    truetypefontcache.h
)
