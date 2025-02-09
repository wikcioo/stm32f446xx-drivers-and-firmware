#include "stm32f446xx_gpio.h"

static void gpio_clock_control(gpio_regdef_t *gpiox, uint8_t state);

void gpio_init(gpio_handle_t *gpio_handle)
{
    gpio_clock_control(gpio_handle->gpiox, ENABLE);

    uint32_t temp_reg = 0;

    /* Configure GPIO pin output type */
    temp_reg = gpio_handle->config.pin_output_type << gpio_handle->config.pin_number;
    gpio_handle->gpiox->OTYPER &= ~(0x1 << gpio_handle->config.pin_number);
    gpio_handle->gpiox->OTYPER |= temp_reg;

    /* Configure GPIO pin speed */
    temp_reg = gpio_handle->config.pin_speed << (2 * gpio_handle->config.pin_number);
    gpio_handle->gpiox->OSPEEDR &= ~(0x3 << (2 * gpio_handle->config.pin_number));
    gpio_handle->gpiox->OSPEEDR |= temp_reg;

    /* Configure GPIO pin pull-up/pull-down */
    temp_reg = gpio_handle->config.pin_pupd << (2 * gpio_handle->config.pin_number);
    gpio_handle->gpiox->PUPDR &= ~(0x3 << 2 * gpio_handle->config.pin_number);
    gpio_handle->gpiox->PUPDR |= temp_reg;

    /* Configure GPIO pin alternate functionality */
    if (gpio_handle->config.pin_mode == GPIO_MODE_ALT_FUNC)
    {
        uint8_t reg_index = gpio_handle->config.pin_number / 8;
        uint8_t bit_pos   = gpio_handle->config.pin_number % 8;
        gpio_handle->gpiox->AFR[reg_index] &= ~(0xF << 4 * bit_pos);
        gpio_handle->gpiox->AFR[reg_index] |= gpio_handle->config.pin_alt_func << (4 * bit_pos);
    }

    /* Configure GPIO pin mode */
    if (gpio_handle->config.pin_mode <= GPIO_MODE_ANALOG)
    {
        temp_reg = gpio_handle->config.pin_mode << (2 * gpio_handle->config.pin_number);
        gpio_handle->gpiox->MODER &= ~(0x3 << (2 * gpio_handle->config.pin_number));
        gpio_handle->gpiox->MODER |= temp_reg;
    }
    else
    {
        /* Interrupt mode */
        if (gpio_handle->config.pin_mode == GPIO_MODE_IT_RE)
        {
            EXTI->RTSR |=  (1 << gpio_handle->config.pin_number);
            EXTI->FTSR &= ~(1 << gpio_handle->config.pin_number);
        }
        else if (gpio_handle->config.pin_mode == GPIO_MODE_IT_FE)
        {
            EXTI->RTSR &= ~(1 << gpio_handle->config.pin_number);
            EXTI->FTSR |=  (1 << gpio_handle->config.pin_number);
        }
        else if (gpio_handle->config.pin_mode == GPIO_MODE_IT_RFE)
        {
            EXTI->RTSR |= (1 << gpio_handle->config.pin_number);
            EXTI->FTSR |= (1 << gpio_handle->config.pin_number);
        }

        EXTI->IMR |= (1 << gpio_handle->config.pin_number);

        SYSCFG_CLK_ENABLE();
        uint8_t index  = gpio_handle->config.pin_number / 4;
        uint8_t offset = gpio_handle->config.pin_number % 4;
        SYSCFG->EXTICR[index] &= ~(0xF << (4 * offset));
        SYSCFG->EXTICR[index] |= GPIO_CODE(gpio_handle->gpiox) << (4 * offset);
    }
}

uint8_t gpio_read_pin(gpio_regdef_t *gpiox, uint8_t pin_number)
{
    return (uint8_t)((gpiox->IDR >> pin_number) & 1);
}

uint16_t gpio_read_port(gpio_regdef_t *gpiox)
{
    return (uint16_t)(gpiox->IDR & 0xFFFF);
}

void gpio_write_pin(gpio_regdef_t *gpiox, uint8_t pin_number, uint8_t value)
{
    if (value == GPIO_PIN_HIGH)
        gpiox->ODR |= 1 << pin_number;
    else
        gpiox->ODR &= ~(1 << pin_number);
}

void gpio_write_port(gpio_regdef_t *gpiox, uint16_t value)
{
    if (value == GPIO_PIN_HIGH)
        gpiox->ODR |= 0xFFFF;
    else
        gpiox->ODR &= ~(0xFFFF);
}

void gpio_toggle_pin(gpio_regdef_t *gpiox, uint8_t pin_number)
{
    gpiox->ODR ^= 1 << pin_number;
}

void gpio_irq_enable(irq_nr number)
{
    nvic_enable_irq(number);
}

void gpio_irq_disable(irq_nr number)
{
    nvic_disable_irq(number);
}

void gpio_irq_priority(irq_nr number, irq_priority priority)
{
    nvic_set_priority(number, priority);
}

void gpio_irq_handler(uint8_t pin_number)
{
    if (EXTI->PR & (1 << pin_number))
        EXTI->PR |= 1 << pin_number;
}

static void gpio_clock_control(gpio_regdef_t *gpiox, uint8_t state)
{
    if (state == ENABLE)
    {
        if (gpiox == GPIOA) GPIOA_CLK_ENABLE();
        else if (gpiox == GPIOB) GPIOB_CLK_ENABLE();
        else if (gpiox == GPIOC) GPIOC_CLK_ENABLE();
        else if (gpiox == GPIOD) GPIOD_CLK_ENABLE();
        else if (gpiox == GPIOE) GPIOE_CLK_ENABLE();
        else if (gpiox == GPIOF) GPIOF_CLK_ENABLE();
        else if (gpiox == GPIOG) GPIOG_CLK_ENABLE();
        else if (gpiox == GPIOH) GPIOH_CLK_ENABLE();
    }
    else if (state == DISABLE)
    {
        if (gpiox == GPIOA) GPIOA_CLK_DISABLE();
        else if (gpiox == GPIOB) GPIOB_CLK_DISABLE();
        else if (gpiox == GPIOC) GPIOC_CLK_DISABLE();
        else if (gpiox == GPIOD) GPIOD_CLK_DISABLE();
        else if (gpiox == GPIOE) GPIOE_CLK_DISABLE();
        else if (gpiox == GPIOF) GPIOF_CLK_DISABLE();
        else if (gpiox == GPIOG) GPIOG_CLK_DISABLE();
        else if (gpiox == GPIOH) GPIOH_CLK_DISABLE();
    }
}
