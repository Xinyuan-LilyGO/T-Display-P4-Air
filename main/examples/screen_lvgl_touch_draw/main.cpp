/*
 * @Description: None
 * @Author: LILYGO_L
 * @Date: 2026-02-02 17:36:09
 * @LastEditTime: 2026-03-12 17:21:12
 * @License: GPL 3.0
 */
#include "lilygo_device_driver_library.h"
#include "cpp_bus_driver_library.h"
#include "lvgl.h"

#define LVGL_TICK_PERIOD_MS 1

// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

// 定义一个画布对象
static lv_obj_t *canvas;
static lv_layer_t layer;

// 定义一个计时器，用于检测 5 秒无操作
time_t last_touch_time = 0;

// 定义一个标志，用于判断是否需要清除画布
bool need_clear_lock_flag = false;

std::vector<uint16_t> Lvgl_Draw_X_Data;
std::vector<uint16_t> Lvgl_Draw_Y_Data;

lv_point_t point;

auto Axp517_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(AXP517_SDA, AXP517_SCL, I2C_NUM_0);
auto Xl9555_Iic_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(XL9555_SDA, XL9555_SCL, I2C_NUM_1);
auto Mipi_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Mipi>(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_MIPI_DSI_HSYNC, SCREEN_MIPI_DSI_HBP, SCREEN_MIPI_DSI_HFP,
                                                                SCREEN_MIPI_DSI_VSYNC, SCREEN_MIPI_DSI_VBP, SCREEN_MIPI_DSI_VFP, SCREEN_DATA_LANE_NUM,
                                                                [](uint8_t format) -> Cpp_Bus_Driver::Hardware_Mipi::Color_Format
                                                                {
                                                                    switch (format)
                                                                    {
                                                                    case 16:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB565;
                                                                    case 24:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB888;
                                                                    default:
                                                                        return Cpp_Bus_Driver::Hardware_Mipi::Color_Format::RGB565;
                                                                    } }(SCREEN_BITS_PER_PIXEL));
auto Touch_Bus = std::make_shared<Cpp_Bus_Driver::Hardware_Iic_1>(HI8561_TOUCH_SDA, HI8561_TOUCH_SCL, I2C_NUM_0);

auto Axp517 = std::make_unique<Cpp_Bus_Driver::Axp517>(Axp517_Iic_Bus, AXP517_IIC_ADDRESS);
auto Xl9555 = std::make_unique<Cpp_Bus_Driver::Xl95x5>(Xl9555_Iic_Bus, XL9555_IIC_ADDRESS);
auto Screen = std::make_unique<Cpp_Bus_Driver::Hi8561>(Mipi_Bus);
auto Touch = std::make_unique<Cpp_Bus_Driver::Hi8561_Touch>(Touch_Bus, HI8561_TOUCH_IIC_ADDRESS);

auto Esp32p4 = std::make_unique<Cpp_Bus_Driver::Tool>();

extern "C" void example_lvgl_demo_ui(lv_display_t *disp);

void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    Cpp_Bus_Driver::Hi8561_Touch::Touch_Point tp;

    if (Touch->get_single_touch_point(tp) == true)
    {
        printf("touch finger: %d edge touch flag: %d\nx: %d y: %d p: %d\n",
               tp.finger_count, tp.edge_touch_flag, tp.info[0].x, tp.info[0].y, tp.info[0].pressure_value);

        data->state = LV_INDEV_STATE_PR;

        /*Set the coordinates*/
        data->point.x = tp.info[0].x;
        data->point.y = tp.info[0].y;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lvgl_ui_task(void *arg)
{
    printf("lvgl_ui_task start\n");
    uint32_t time_till_next_ms = 0;

    while (1)
    {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);

        // in case of task watch dog timeout, set the minimal delay to 10ms
        if (time_till_next_ms < 10)
        {
            time_till_next_ms = 10;
        }
        usleep(1000 * time_till_next_ms);

        // lv_timer_handler();
        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Lvgl_Init(void)
{
    printf("initialize lvgl\n");

    lv_init();

    // create a lvgl display
    lv_display_t *display = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, Screen.get());
    // set color depth
    lv_display_set_color_format(display, [](uint8_t format) -> lv_color_format_t
                                {
                                    switch (format)
                                    {
                                    case 16:
                                        return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
                                    case 24:
                                        return lv_color_format_t::LV_COLOR_FORMAT_RGB888;
                                    default:
                                        return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
                                    } }(SCREEN_BITS_PER_PIXEL));
    // create draw buffer
    printf("allocate separate lvgl draw buffers\n");
    size_t draw_buffer_sz = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
    void *buf1 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    assert(buf1);
    // void *buf2 = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    // assert(buf2);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, [](lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
                            {
                                auto Screen = (Cpp_Bus_Driver::Hi8561 *)lv_display_get_user_data(disp);

                                esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
                                int offsetx1 = area->x1;
                                int offsetx2 = area->x2;
                                int offsety1 = area->y1;
                                int offsety2 = area->y2;
                                // pass the draw buffer to the driver
                                Screen->send_color_stream_coordinate(offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map); });

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);

    printf("register dpi panel event callback for lvgl flush ready notification\n");
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = [](esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) -> bool
        {
            lv_display_t *disp = (lv_display_t *)user_ctx;
            lv_display_flush_ready(disp);
            return false; },
        .on_refresh_done = [](esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) -> bool
        {
            // static int io_level = 0;
            // // please note, the real refresh rate should be 2*frequency of this GPIO toggling
            // gpio_set_level(EXAMPLE_PIN_NUM_REFRESH_MONITOR, io_level);
            // io_level = !io_level;
            return false; },
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(Mipi_Bus->get_device_handle(), &cbs, display));

    printf("use esp_timer as lvgl tick timer\n");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void *arg)
        {
            lv_tick_inc(LVGL_TICK_PERIOD_MS);
        },
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));
}

// 绘图回调函数
void draw_point(lv_event_t *e)
{
    // lv_obj_t *obj = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    // printf("code: %d\n", code);

    switch (code)
    {
    case LV_EVENT_PRESSING:
    {
        lv_indev_t *indev = lv_indev_get_act();
        lv_indev_get_point(indev, &point);

        // printf("touch x: %ld y: %ld\n", point.x, point.y);

        // 在画布上绘制点
        // lv_canvas_set_px(canvas, point.x, point.y, lv_palette_main(LV_PALETTE_RED), LV_OPA_COVER);

        Lvgl_Draw_X_Data.push_back(point.x);
        Lvgl_Draw_Y_Data.push_back(point.y);

        if ((Lvgl_Draw_X_Data.size() >= 2) && (Lvgl_Draw_Y_Data.size() >= 2))
        {
            lv_draw_line_dsc_t dsc;
            lv_draw_line_dsc_init(&dsc);
            dsc.color = lv_palette_main(LV_PALETTE_RED);
            dsc.width = 4;
            dsc.round_end = 1;
            dsc.round_start = 1;
            dsc.p1.x = Lvgl_Draw_X_Data[0];
            dsc.p1.y = Lvgl_Draw_Y_Data[0];
            dsc.p2.x = Lvgl_Draw_X_Data[1];
            dsc.p2.y = Lvgl_Draw_Y_Data[1];
            lv_draw_line(&layer, &dsc);

            lv_canvas_finish_layer(canvas, &layer);

            Lvgl_Draw_X_Data.erase(Lvgl_Draw_X_Data.begin());
            Lvgl_Draw_Y_Data.erase(Lvgl_Draw_Y_Data.begin());
        }

        // 获取当前时间
        time_t current_time = time(NULL);
        // 更新上次触摸时间
        last_touch_time = current_time;
        need_clear_lock_flag = true;
    }
    break;
    case LV_EVENT_RELEASED:
        //  printf("777\n");
        Lvgl_Draw_X_Data.clear();
        Lvgl_Draw_Y_Data.clear();
        break;

    default:
        break;
    }
}

void lv_example_canvas_7(void)
{
    void *draw_buf = NULL;
    size_t draw_buffer_sz = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
    draw_buf = heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM);

    /*Create a canvas and initialize its palette*/
    canvas = lv_canvas_create(lv_screen_active());
    // lv_canvas_set_draw_buf(canvas, (lv_draw_buf_t *)draw_buf);
    lv_canvas_set_buffer(canvas, draw_buf, SCREEN_WIDTH, SCREEN_HEIGHT, [](uint8_t format) -> lv_color_format_t
                         {
                            switch (format)
                            {
                            case 16:
                                return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
                            case 24:
                                return lv_color_format_t::LV_COLOR_FORMAT_RGB888;
                            default:
                                return lv_color_format_t::LV_COLOR_FORMAT_RGB565;
                            } }(SCREEN_BITS_PER_PIXEL));
    lv_canvas_fill_bg(canvas, lv_color_hex3(0xCCC), LV_OPA_COVER);
    lv_obj_center(canvas);

    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_palette_main(LV_PALETTE_RED);
    dsc.width = 4;
    dsc.round_end = 1;
    dsc.round_start = 1;
    dsc.p1.x = 15;
    dsc.p1.y = 15;
    dsc.p2.x = 35;
    dsc.p2.y = 10;
    lv_draw_line(&layer, &dsc);

    lv_canvas_finish_layer(canvas, &layer);

    // 注册触摸事件回调
    lv_obj_add_event_cb(lv_screen_active(), draw_point, LV_EVENT_ALL, NULL);
}

extern "C" void app_main(void)
{
    printf("Ciallo\n");

    Axp517->begin();

    Xl9555->begin();

    Xl9555->pin_mode(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);

    Xl9555->pin_write(XL9555_3_3_POWER_EN, Cpp_Bus_Driver::Xl95x5::Value::HIGH);

    Lilygo_Device_Driver::Init_Ldo_Channel_Power(3, 2500);

    vTaskDelay(pdMS_TO_TICKS(100));

    Xl9555->pin_mode(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_mode(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Mode::OUTPUT);
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(5));
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::LOW);
    vTaskDelay(pdMS_TO_TICKS(10));
    Xl9555->pin_write(XL9555_SCREEN_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    Xl9555->pin_write(XL9555_TOUCH_RST, Cpp_Bus_Driver::Xl95x5::Value::HIGH);
    vTaskDelay(pdMS_TO_TICKS(120));

    Esp32p4->create_pwm(HI8561_SCREEN_BL, ledc_channel_t::LEDC_CHANNEL_0, 2000);

    Screen->begin(SCREEN_MIPI_DSI_DPI_CLK_MHZ, SCREEN_LANE_BIT_RATE_MBPS);

    Touch_Bus->set_bus_handle(Axp517_Iic_Bus->get_bus_handle());

    Touch->begin();

    Lvgl_Init();
    xTaskCreate(lvgl_ui_task, "lvgl_ui_task", 100 * 1024, NULL, 1, NULL);

    lv_example_canvas_7();

    Esp32p4->start_pwm_gradient_time(100, 500);
}
