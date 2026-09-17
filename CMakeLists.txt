if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_IMPRINT_RGB_LAYER_COLORS app PRIVATE src/rgb_layer_colors.c)
endif()

target_include_directories(app PRIVATE include)
target_sources(app PRIVATE src/rgb_idle_timeout.c)
