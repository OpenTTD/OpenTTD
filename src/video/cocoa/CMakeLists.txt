add_files(
    cocoa_keys.h
    cocoa_v.h
    cocoa_v.mm
    cocoa_wnd.h
    cocoa_wnd.mm
    CONDITION APPLE
)

add_files(
    cocoa_ogl.h
    cocoa_ogl.mm
    CONDITION APPLE AND OPENGL_FOUND
)
