# Only optional modules go here.

nth_register(window
    DEFAULT ON
    DESCRIPTION "OS window and input")

nth_register(vulkan
    DEFAULT ON
    DEPENDS window
    DESCRIPTION "Vulkan render backend")