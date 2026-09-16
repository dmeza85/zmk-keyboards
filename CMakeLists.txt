if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_IMPRINT_RGB_LAYER_COLORS app PRIVATE src/rgb_layer_colors.c)
endif()
