# ESP-IDF 컴포넌트 정의
# 사용법: 프로젝트 components/cuime/ 로 심볼릭 링크하거나
#         set(EXTRA_COMPONENT_DIRS ".../cheonjiin/firmware")
idf_component_register(
    SRCS "hal_esp32.c"
         "../../app/cuime_app.c"
         "../../core/cuime.c"
         "main_esp32.c"
    INCLUDE_DIRS "../../core" "../../hal" "../../app"
    REQUIRES driver nvs_flash esp_timer bt
)
