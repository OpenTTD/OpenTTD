add_files(
    khrplatform.h
    glext.h
    CONDITION OPENGL_FOUND
)

add_files(
    wglext.h
    CONDITION WIN32 AND OPENGL_FOUND
)
