#define DMOD_ENABLE_REGISTRATION ON
#include <errno.h>
#include <string.h>
#include "dmod.h"
#include "dmosi.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmspiflash.h"
#include "dmspiflash_internal.h"

/*
 * Module entry, pure helpers and the dmdrvi driver ("driver_name=dmspiflash"
 * in the board .ini): a block device on a flash chip behind a bus node.
 * Registration (DMOD_ENABLE_REGISTRATION) lives only in this file.
 *
 * The bus is found through the friends_group (the friend with friend_role=bus,
 * see _friend_changed) or `bus_path=` in the .ini, and opened, with the chip
 * identified, on first use (ensure_ready) - the bus driver may appear after us.
 */

#define SECTION_DEFAULT     "dmspiflash"
#define LOGICAL_BLOCK       512u

dmod_dmspiflash_api_declaration(1.0, bool, _validate_config, ( const dmspiflash_config_t *config ))
{
    if (config == NULL)
        return false;
    if (config->bus != dmspiflash_bus_spi && config->bus != dmspiflash_bus_qspi)
        return false;
    if (config->bus == dmspiflash_bus_spi && (config->use_quad || config->memory_mapped))
        return false;       /* quad and XIP need the qspi bus */
    return true;
}

dmod_dmspiflash_api_declaration(1.0, uint32_t, _page_chunk, ( uint32_t page_size, uint32_t offset, uint32_t remaining ))
{
    if (page_size == 0)
        return 0;
    uint32_t room = page_size - (offset % page_size);
    return remaining < room ? remaining : room;
}

int dmod_init(const Dmod_Config_t *Config)
{
    (void)Config;
    Dmod_Printf("dmspiflash: NOR flash driver initialized\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}

/* ---- Configuration ---- */

static bool section_is_ours(dmini_context_t ini, const char *section)
{
    return dmini_has_key(ini, section, "bus");
}

static const char *detect_section(dmini_context_t ini, char *buf, size_t buf_size)
{
    if (section_is_ours(ini, SECTION_DEFAULT))
        return SECTION_DEFAULT;
    int needed = dmini_generate_string(ini, NULL, 0);
    char *text = needed > 1 ? (char *)Dmod_Malloc((size_t)needed + 1) : NULL;
    if (text == NULL || dmini_generate_string(ini, text, (size_t)needed + 1) <= 0)
    {
        Dmod_Free(text);
        return SECTION_DEFAULT;
    }
    text[needed] = '\0';
    const char *result = SECTION_DEFAULT;
    for (char *p = text; *p != '\0' && result == SECTION_DEFAULT; p++)
    {
        if (*p != '[')
            continue;
        char *end = p + 1;
        while (*end != '\0' && *end != ']' && *end != '\n')
            end++;
        size_t len = (size_t)(end - p - 1);
        if (*end == ']' && len > 0 && len < buf_size)
        {
            memcpy(buf, p + 1, len);
            buf[len] = '\0';
            if (strcmp(buf, "main") != 0 && section_is_ours(ini, buf))
                result = buf;
        }
    }
    Dmod_Free(text);
    return result;
}

static bool read_config(struct dmdrvi_context *ctx, dmini_context_t ini, const char *section)
{
    const char *bus = dmini_get_string(ini, section, "bus", "");
    if (strcmp(bus, "qspi") == 0)
        ctx->config.bus = dmspiflash_bus_qspi;
    else if (strcmp(bus, "spi") == 0)
        ctx->config.bus = dmspiflash_bus_spi;
    else
    {
        DMOD_LOG_ERROR("dmspiflash: [%s] needs bus=qspi or bus=spi\n", section);
        return false;
    }
    const char *chip = dmini_get_string(ini, section, "chip", "");
    strncpy(ctx->config.chip_name, chip, sizeof(ctx->config.chip_name) - 1);
    ctx->config.use_quad = dmini_get_int(ini, section, "quad", 0) != 0;
    ctx->config.memory_mapped = dmini_get_int(ini, section, "memory_mapped", 0) != 0;
    const char *path = dmini_get_string(ini, section, "bus_path", "");
    if (path[0] != '\0')
        ctx->bus_path = Dmod_StrDup(path);
    return dmspiflash_validate_config(&ctx->config);
}

/* ---- Context ---- */

static bool is_valid(struct dmdrvi_context *ctx)
{
    return ctx != NULL && ctx->magic == DMSPIFLASH_MAGIC;
}

/* Lock held. Open the bus and identify the chip once. */
static int ensure_ready(struct dmdrvi_context *ctx)
{
    if (ctx->ready)
        return 0;
    int ret = dmspiflash_bus_open(ctx);
    if (ret == 0)
        ret = dmspiflash_core_open(ctx);
    if (ret != 0)
    {
        dmspiflash_bus_close(ctx);
        return ret;
    }
    ctx->ready = true;
    return 0;
}

static int lock_ready(struct dmdrvi_context *ctx)
{
    if (!is_valid(ctx))
        return -EINVAL;
    dmosi_mutex_lock(ctx->lock);
    int ret = ensure_ready(ctx);
    if (ret != 0)
        dmosi_mutex_unlock(ctx->lock);
    return ret;
}

/* ---- dmdrvi interface ---- */

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if (config == NULL || dev_num == NULL)
        return NULL;
    struct dmdrvi_context *ctx = Dmod_Malloc(sizeof(*ctx));
    if (ctx == NULL)
        return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->magic = DMSPIFLASH_MAGIC;
    char section_buf[DMDRVI_ALT_NAME_MAX_LEN + 1];
    const char *section = detect_section(config, section_buf, sizeof(section_buf));
    ctx->lock = dmosi_mutex_create(false);
    if (ctx->lock == NULL || !read_config(ctx, config, section))
    {
        Dmod_Free(ctx->bus_path);
        if (ctx->lock != NULL)
            dmosi_mutex_destroy(ctx->lock);
        Dmod_Free(ctx);
        return NULL;
    }
    dev_num->flags = DMDRVI_NUM_MINOR;
    dev_num->major = 0;
    dev_num->minor = 0;
    size_t len = strlen(section);
    if (strcmp(section, SECTION_DEFAULT) != 0 && len <= DMDRVI_ALT_NAME_MAX_LEN)
    {
        dev_num->flags |= DMDRVI_NUM_ALT_NAME;
        memcpy(dev_num->alt_name, section, len + 1);
    }
    return ctx;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, void, _free, ( dmdrvi_context_t context ))
{
    if (!is_valid(context))
        return;
    dmosi_mutex_lock(context->lock);
    if (context->ready)
    {
        if (context->mapped)
            (void)dmspiflash_bus_map(context, false);
        dmspiflash_bus_close(context);
    }
    dmosi_mutex_unlock(context->lock);
    dmosi_mutex_destroy(context->lock);
    Dmod_Free(context->bus_path);
    context->magic = 0;
    Dmod_Free(context);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, void, _friend_changed,
    ( dmdrvi_context_t context, const dmdrvi_friend_info_t* info ))
{
    if (!is_valid(context) || info == NULL || info->friend_role == NULL || strcmp(info->friend_role, "bus") != 0)
        return;
    dmosi_mutex_lock(context->lock);
    if (info->state == dmdrvi_dev_state_ready && info->node_path != NULL && context->bus_path == NULL)
        context->bus_path = Dmod_StrDup(info->node_path);
    else if (info->state != dmdrvi_dev_state_ready && context->ready)
    {
        dmspiflash_bus_close(context);      /* The bus went away: identify again next time */
        context->ready = false;
    }
    dmosi_mutex_unlock(context->lock);
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, void*, _open, ( dmdrvi_context_t context, int flags, const dmdrvi_dev_num_t *dev_num ))
{
    (void)flags; (void)dev_num;
    return is_valid(context) ? context : NULL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    (void)context; (void)handle;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, dmdrvi_ssize_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)handle;
    if (offset < 0 || buffer == NULL)
        return -EINVAL;
    int ret = lock_ready(context);
    if (ret != 0)
        return ret;
    ret = dmspiflash_core_read(context, (uint32_t)offset, buffer, size);
    dmosi_mutex_unlock(context->lock);
    return ret;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, dmdrvi_ssize_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, dmdrvi_offset_t offset ))
{
    (void)handle;
    if (offset < 0 || buffer == NULL)
        return -EINVAL;
    int ret = lock_ready(context);
    if (ret != 0)
        return ret;
    ret = dmspiflash_core_write(context, (uint32_t)offset, buffer, size);
    dmosi_mutex_unlock(context->lock);
    return ret;
}

static int ioctl_locked(struct dmdrvi_context *ctx, int command, void *arg)
{
    const dmspiflash_chip_t *chip = ctx->chip;
    switch (command)
    {
        case DMDRVI_IOCTL_BLOCK_GET_INFO:
        {
            dmdrvi_block_info_t *info = (dmdrvi_block_info_t *)arg;
            info->logical_block_size = LOGICAL_BLOCK;
            info->erase_block_size = chip->sector_size;
            info->block_count = chip->size_bytes / LOGICAL_BLOCK;
            info->flags = DMDRVI_BLOCK_FLAG_ERASE_SUPPORTED;
            return 0;
        }
        case DMDRVI_IOCTL_BLOCK_ERASE:
        {
            const dmdrvi_block_range_t *r = (const dmdrvi_block_range_t *)arg;
            return r->offset < 0 ? -EINVAL : dmspiflash_core_erase(ctx, (uint32_t)r->offset, (uint32_t)r->length);
        }
        case DMSPIFLASH_IOCTL_GET_JEDEC_ID:
            *(dmspiflash_jedec_id_t *)arg = chip->id;
            return 0;
        case DMSPIFLASH_IOCTL_GET_CHIP:
            *(dmspiflash_chip_t *)arg = *chip;
            return 0;
        case DMSPIFLASH_IOCTL_ERASE_CHIP:
            return dmspiflash_core_erase_chip(ctx);
        case DMSPIFLASH_IOCTL_SET_MEMORY_MAPPED:
        {
            bool on = *(const bool *)arg;
            int ret = dmspiflash_bus_map(ctx, on);
            ctx->mapped = (ret == 0) && on;
            return ret;
        }
        case DMSPIFLASH_IOCTL_GET_MAPPED_BASE:
            *(uintptr_t *)arg = ctx->mapped ? dmspiflash_bus_mapped_base(ctx) : 0;
            return ctx->mapped ? 0 : -ENOTSUP;
        default:
            return -ENOTTY;
    }
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    (void)handle;
    bool ours = command == DMDRVI_IOCTL_BLOCK_GET_INFO || command == DMDRVI_IOCTL_BLOCK_ERASE ||
                (command >= DMSPIFLASH_IOCTL_GET_JEDEC_ID && command <= DMSPIFLASH_IOCTL_GET_MAPPED_BASE);
    if (!ours)
        return -ENOTTY;     /* e.g. the class probes dmdevfs sends to every node */
    if (arg == NULL && command != DMSPIFLASH_IOCTL_ERASE_CHIP)
        return -EINVAL;
    int ret = lock_ready(context);
    if (ret != 0)
        return ret;
    ret = ioctl_locked(context, command, arg);
    dmosi_mutex_unlock(context->lock);
    return ret;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    (void)handle;
    return is_valid(context) ? 0 : -EINVAL;
}

dmod_dmdrvi_dif_api_declaration(2.0, dmspiflash, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    (void)path;
    if (!is_valid(context) || stat == NULL)
        return -EINVAL;
    stat->size = (dmdrvi_size_t)(context->ready ? context->chip->size_bytes : 0u);
    stat->mode = 0666;
    return 0;
}
