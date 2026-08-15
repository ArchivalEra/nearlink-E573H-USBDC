/*
 * zdiag_stub.c — stubs for diag channels removed for the 7.x kernel build
 * (zdiag_local_log / zdiag_linux_uart / zdiag_linux_socket use removed
 * set_fs()/VFS APIs). These diagnostics are not needed for the HCC/USB
 * driver bring-up path.
 *
 * Added by the x86 port (ticket 06). SDK original had real implementations.
 */

#include <linux/types.h>
#include <linux/module.h>

int zdiag_uart_channel_exit(void *unused)
{
    (void)unused;
    return 0;
}

int zdiag_socket_channel_exit(void *unused)
{
    (void)unused;
    return 0;
}

int zdiag_sock_output_buf(void *unused1, unsigned int unused2)
{
    (void)unused1;
    (void)unused2;
    return 0;
}

int zdiag_uart_output_buf(void *unused1, unsigned int unused2)
{
    (void)unused1;
    (void)unused2;
    return 0;
}

int zdiag_offline_log_output(void *unused1, unsigned int unused2)
{
    (void)unused1;
    (void)unused2;
    return 0;
}

EXPORT_SYMBOL(zdiag_uart_channel_exit);
EXPORT_SYMBOL(zdiag_socket_channel_exit);
EXPORT_SYMBOL(zdiag_sock_output_buf);
EXPORT_SYMBOL(zdiag_uart_output_buf);
EXPORT_SYMBOL(zdiag_offline_log_output);

/* ---- init entries referenced by zdiag_adapt_os.c / plat_main.c ---- */
int zdiag_local_log_init(void *unused)
{
    (void)unused;
    return 0;
}
EXPORT_SYMBOL(zdiag_local_log_init);

int zdiag_uart_channel_init(void *unused)
{
    (void)unused;
    return 0;
}
EXPORT_SYMBOL(zdiag_uart_channel_init);

int zdiag_socket_channel_init(void *unused)
{
    (void)unused;
    return 0;
}
EXPORT_SYMBOL(zdiag_socket_channel_init);

/* ---- plat_pm_dfr.c removed: panic memdump state queries ---- */
int plat_dfr_status_get(void)
{
    return 0;
}
EXPORT_SYMBOL(plat_dfr_status_get);

int plat_dfr_wait_flag_get(void)
{
    return 0;
}
EXPORT_SYMBOL(plat_dfr_wait_flag_get);

void plat_dfr_wait_flag_set(int unused)
{
    (void)unused;
}
EXPORT_SYMBOL(plat_dfr_wait_flag_set);

/* ---- plat_pm_dfr.c / plat_main.c / zdiag: remaining referenced exports ---- */
void plat_exception_exit(void) { }
EXPORT_SYMBOL(plat_exception_exit);

void wlan_set_dfr_recovery_flag(unsigned char dfr_flag)
{
    (void)dfr_flag;
}
EXPORT_SYMBOL(wlan_set_dfr_recovery_flag);

void plat_dfr_lock(void) { }
EXPORT_SYMBOL(plat_dfr_lock);

void plat_dfr_unlock(void) { }
EXPORT_SYMBOL(plat_dfr_unlock);

void plat_dfr_enable_set(bool enable)
{
    (void)enable;
}
EXPORT_SYMBOL(plat_dfr_enable_set);

void plat_dfr_status_set(int dfr_status)
{
    (void)dfr_status;
}
EXPORT_SYMBOL(plat_dfr_status_set);

void zdiag_local_log_cb_for_dev_bsp_ready(void) { }
EXPORT_SYMBOL(zdiag_local_log_cb_for_dev_bsp_ready);

/* ---- plat_pm_dfr.c excluded: exception/panic-memdump entry points ---- */
int plat_exception_init(void)
{
    return 0;
}
EXPORT_SYMBOL(plat_exception_init);

unsigned int plat_exception_reset_process(bool should_dump_trace)
{
    (void)should_dump_trace;
    return 0;
}
EXPORT_SYMBOL(plat_exception_reset_process);

unsigned char plat_is_device_in_recovery(void)
{
    return 0;
}
EXPORT_SYMBOL(plat_is_device_in_recovery);

void plat_update_device_recovery_flag(unsigned char flag)
{
    (void)flag;
}
EXPORT_SYMBOL(plat_update_device_recovery_flag);

void plat_dfr_trigger_panic(void) { }
EXPORT_SYMBOL(plat_dfr_trigger_panic);

void *open_file_to_readm_etc(void *name, int flags)
{
    (void)name;
    (void)flags;
    return NULL;
}
EXPORT_SYMBOL(open_file_to_readm_etc);

int recv_device_mem_etc(void *fp, unsigned char *puc_data_buf, int len)
{
    (void)fp;
    (void)puc_data_buf;
    return len > 0 ? len : -1;
}
EXPORT_SYMBOL(recv_device_mem_etc);
