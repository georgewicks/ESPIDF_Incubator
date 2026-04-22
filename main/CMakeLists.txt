# OLD:
#idf_component_register(SRCS "WiFi.c" "MCP9808.c" "main.c"
#                    INCLUDE_DIRS "." "include")

# NEW
set(COMPONENT_SRCS "main.c" "MCP9808.c" "TempControl.c" "WiFi.c" )
set(COMPONENT_ADD_INCLUDEDIRS "." "include")

register_component()
