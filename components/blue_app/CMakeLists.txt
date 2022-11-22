idf_component_register(SRCS "blue_app.c"
                    PRIV_REQUIRES nvs_flash bt app_update
                    INCLUDE_DIRS "include")
