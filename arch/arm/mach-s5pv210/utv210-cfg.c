#include <linux/kernel.h>
#include <linux/init.h>
#include <mach/utv210-cfg.h>

char g_LCD[16];
char g_Model[16];
char g_Camera[16];

static void getCmdLineEntry(char *name, char *out, unsigned int size) {
    const char *cmd = saved_command_line;
    const char *ptr = name;

    for (;;) {
        for (; *cmd && *ptr != *cmd; cmd++) ;
        for (; *cmd && *ptr && *ptr == *cmd; ptr++, cmd++) ;

        if (!*cmd) {
            *out = 0;
            return;
        } else if (!*ptr && *cmd == '=') {
            cmd++;
            break;
        } else {
            ptr = name;
            continue;
        }
    }

    for (; --size && *cmd && *cmd != ' '; cmd++, out++)
        *out = *cmd;

    *out = 0;
}

void utv210_init_cfg(void) {
    getCmdLineEntry("lcd", g_LCD, sizeof(g_LCD));
    getCmdLineEntry("utmodel", g_Model, sizeof(g_Model));
    getCmdLineEntry("camera", g_Camera, sizeof(g_Camera));

    printk("Got lcd=%s, utmodel=%s, camera=%s...\n", g_LCD, g_Model, g_Camera);
}

