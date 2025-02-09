#include "stm32f446xx_flash.h"
#include "stm32f446xx_rcc.h"

#define FLASH_KEY1 (0x45670123)
#define FLASH_KEY2 (0xCDEF89AB)

#define FLASH_OPT_KEY1 (0x08192A3B)
#define FLASH_OPT_KEY2 (0x4C5D6E7F)

static void flash_lock(void);
static void flash_unlock(void);

static void flash_opt_lock(void);
static void flash_opt_unlock(void);

static void wait_for_not_busy(void);

void flash_init(void)
{
    uint32_t system_clock = rcc_get_system_clock_freq();

    // Reset the last 4 bits so that WS = 0 (default behaviour)
    FLASH->ACR &= ~(0xF << FLASH_ACR_LATENCY);

    // The following calculates the WS value based on the system clock frequency
    // For now it is assumed that the internal voltage reference is 3.3V,
    // which makes the step value between frequencies to be 30
    // TODO: Implement this function for other voltage values

    uint8_t step = 30; // hardcoded for now
    uint8_t ws_value = ((system_clock / MEGA) - 1) / step;

    // Set the last 4 bits to the ws_value
    FLASH->ACR |= (ws_value & 0xF) << FLASH_ACR_LATENCY;

    // Hardcoded value for maximum parallelism size for write operations at 3.3V: x32
    // Used during erase operations of the flash memory
    // TODO: Unhardcode later
    flash_unlock();
    FLASH->CR |= 2 << FLASH_CR_PSIZE;
    flash_lock();
}

uint8_t flash_read(uint32_t address, uint8_t *rx_buffer, uint32_t length)
{
    if (!(address >= FLASH_SECTOR_0_BASE_ADDR && address <= FLASH_END_ADDR) ||
         (length > FLASH_END_ADDR - address))
    {
        return FLASH_FAIL;
    }

    uint8_t *flash_ptr = (uint8_t *) address;

    /* Copy length bytes of data from flash memory to the buffer */
    while (length--)
    {
        *rx_buffer++ = *flash_ptr++;
    }

    return FLASH_SUCCESS;
}

void flash_write(uint32_t address, uint8_t *data, uint32_t length)
{
    flash_unlock();

    /* Activate programming mode */
    FLASH->CR |= 1 << FLASH_CR_PG;
    
    // Hardcoded to 32bits because of x32 parallelism
    uint32_t *data_ptr = (uint32_t *) data;
    uint32_t no_bytes = (length % 4 == 0 ? (length / 4) : ((length / 4) + 1));
    uint32_t *flash_ptr = (uint32_t *) address;

    while (no_bytes--)
    {
        *flash_ptr++ = *data_ptr++;
    }

    wait_for_not_busy();

    flash_lock();
}

void flash_sector_erase(uint8_t sector_number)
{
    flash_unlock();

    /* Clear the first 7 bits which contain current flash configuration */
    FLASH->CR &= ~0x7F;

    /* Set the sector to be erased */
    FLASH->CR |= (sector_number & 0xF) << FLASH_CR_SNB;

    /* Activate sector erasing */
    FLASH->CR |= 1 << FLASH_CR_SER;

    /* Start erasing */
    FLASH->CR |= 1 << FLASH_CR_STRT;

    wait_for_not_busy();

    flash_lock();
}

void flash_mass_erase(void)
{
    flash_unlock();

    /* Clear the first 7 bits which contain current flash configuration */
    FLASH->CR &= ~0x7F;

    /* Activate mass erase */
    FLASH->CR |= 1 << FLASH_CR_MER;

    /* Start erasing */
    FLASH->CR |= 1 << FLASH_CR_STRT;

    wait_for_not_busy();

    flash_lock();
}

void flash_get_protection_level(uint8_t prot_level[8])
{
    uint8_t pcrop = (uint8_t) (FLASH->OPTCR >> FLASH_OPTCR_SPRMOD & 1);
    uint8_t n_wrp = (uint8_t) (FLASH->OPTCR >> FLASH_OPTCR_NWRP & 0xFF);

    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t level = FLASH_PROT_NONE;

        /* Read and write protection when pcrop is 1 and the bit in nWRP is 1 */
        if ( (pcrop == FLASH_PROT_MODE_ON) && (n_wrp & (1 << i)) )
        {
            level = FLASH_PROT_READ_WRITE;
        }
        /* Write protection when pcrop is 0 and the bit in nWRP is 0 */
        else if ( (pcrop == FLASH_PROT_MODE_OFF) && (!(n_wrp & (1 << i))) )
        {
            level = FLASH_PROT_WRITE;
        }

        prot_level[i] = level;
    }
}

/*
 * Example usage to set the write protection of sectors 0 and 1
 * flash_set_protection_level(FLASH_PROT_WRITE, FLASH_SECTOR_0 | FLASH_SECTOR_1)
 */
void flash_set_protection_level(uint8_t prot_level, uint8_t sectors)
{
    flash_opt_unlock();

    if (prot_level == FLASH_PROT_READ_WRITE)
    {
        /* Enable PCROP */
        /* nWRP bit 0 is not active, 1 is active*/
        FLASH->OPTCR |= 1 << FLASH_OPTCR_SPRMOD;

        /* Clear nWRP, thus setting all sectors as not active */
        FLASH->OPTCR &= ~(0xFF << FLASH_OPTCR_NWRP);
        /* Set the bits which are to be active */
        FLASH->OPTCR |= sectors << FLASH_OPTCR_NWRP;
    }
    else if (prot_level == FLASH_PROT_WRITE)
    {
        /* Disable PCROP */
        /* nWRP bit 0 is active, 1 is not active*/
        FLASH->OPTCR &= ~(1 << FLASH_OPTCR_SPRMOD);

        /* Set nWRP to 0xFF, thus not active */
        FLASH->OPTCR |= 0xFF << FLASH_OPTCR_NWRP;
        /* Invert the bits of 'sectors' to clear bits which are to be active */
        FLASH->OPTCR &= ~(sectors << FLASH_OPTCR_NWRP);
    }
    else
    {
        /* Disable PCROP */
        /* nWRP bit 0 is active, 1 is not active*/
        FLASH->OPTCR &= ~(1 << FLASH_OPTCR_SPRMOD);

        /* Set nWRP to 0xFF, thus not active */
        FLASH->OPTCR |= sectors << FLASH_OPTCR_NWRP;
    }

    /* Apply the changes */
    FLASH->OPTCR |= 1 << FLASH_OPTCR_OPTSTRT;

    wait_for_not_busy();

    flash_opt_lock();
}

uint8_t flash_is_status_bit_set(uint8_t bit_position)
{
    return (FLASH->SR & (1 << bit_position)) ? SET : RESET;
}

static void flash_lock(void)
{
    FLASH->CR |= (1 << FLASH_CR_LOCK);
}

static void flash_unlock(void)
{
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;

    wait_for_not_busy();
}

static void flash_opt_lock(void)
{
    FLASH->OPTCR |= (1 << FLASH_OPTCR_OPTLOCK);
}

static void flash_opt_unlock(void)
{
    FLASH->OPTKEYR = FLASH_OPT_KEY1;
    FLASH->OPTKEYR = FLASH_OPT_KEY2;

    wait_for_not_busy();
}

static void wait_for_not_busy(void)
{
    while (flash_is_status_bit_set(FLASH_SR_BSY));
}
