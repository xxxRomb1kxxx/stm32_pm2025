#include <stdint.h>
#include <stm32f10x.h>

typedef struct {
    GPIO_TypeDef* port;
    uint32_t pin;
} PinConfig;

static const PinConfig STATUS_LED = {GPIOC, 13};
static const PinConfig UP_BUTTON = {GPIOA, 0};
static const PinConfig DOWN_BUTTON = {GPIOA, 1};

typedef struct {
    uint16_t divider;
    uint16_t reload_value;
} TimerConfig;

static TimerConfig blink_timer = {7200, 10000};

typedef struct {
    uint8_t up_previous;
    uint8_t down_previous;
} ButtonState;

static ButtonState button_status = {1, 1};

static void setup_gpio(void) {
    RCC->APB2ENR |= (RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_AFIOEN);
    
    STATUS_LED.port->CRH = (STATUS_LED.port->CRH & ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13)) 
                          | GPIO_CRH_MODE13_0;
    STATUS_LED.port->BSRR = (1U << STATUS_LED.pin);
    
    uint32_t temp = UP_BUTTON.port->CRL;
    temp &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0 | GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    temp |= (GPIO_CRL_CNF0_1 | GPIO_CRL_CNF1_1);
    UP_BUTTON.port->CRL = temp;
    
    UP_BUTTON.port->ODR |= (1U << UP_BUTTON.pin) | (1U << DOWN_BUTTON.pin);
}

static void apply_timer_settings(uint16_t divider) {
    TIM2->PSC = divider;
    TIM2->EGR = TIM_EGR_UG;
}

static void configure_timer(uint16_t divider, uint16_t reload) {
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    
    apply_timer_settings(divider);
    TIM2->ARR = reload;
    
    TIM2->DIER |= TIM_DIER_UIE;
    
    NVIC->ISER[0] |= (1 << 28); // TIM2_IRQn = 28
    

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        STATUS_LED.port->ODR ^= (1U << STATUS_LED.pin);
    }
}

static uint8_t is_button_active(const PinConfig* button) {
    return (button->port->IDR & (1U << button->pin)) == 0;
}

static void process_button_events(void) {
    uint8_t up_current = is_button_active(&UP_BUTTON);
    uint8_t down_current = is_button_active(&DOWN_BUTTON);
    
    if (up_current && !button_status.up_previous) {
        uint16_t new_divider;
        
        if (blink_timer.divider < 32768) {
            new_divider = blink_timer.divider * 2;
        } else {
            new_divider = 65535;
        }
        
        if (new_divider != blink_timer.divider) {
            blink_timer.divider = new_divider;
            apply_timer_settings(blink_timer.divider);
        }
    }
    
    if (down_current && !button_status.down_previous) {
        uint16_t new_divider;
        
        if (blink_timer.divider > 2) {
            new_divider = blink_timer.divider / 2;
        } else {
            new_divider = 1;
        }
        
        if (new_divider != blink_timer.divider) {
            blink_timer.divider = new_divider;
            apply_timer_settings(blink_timer.divider);
        }
    }
    
    button_status.up_previous = up_current;
    button_status.down_previous = down_current;
}

int main(void) {
    setup_gpio();
    configure_timer(blink_timer.divider - 1, blink_timer.reload_value - 1);
    
    // Основной цикл программы
    while (1) {
        process_button_events();
    }
}
