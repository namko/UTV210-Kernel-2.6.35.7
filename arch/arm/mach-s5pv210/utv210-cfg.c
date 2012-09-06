#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <mach/utv210-cfg.h>

char g_LCD[16];
char g_Model[16];
char g_Camera[16];
char g_Manufacturer[16];

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
    getCmdLineEntry("man", g_Manufacturer, sizeof(g_Manufacturer));
    getCmdLineEntry("lcd", g_LCD, sizeof(g_LCD));
    getCmdLineEntry("utmodel", g_Model, sizeof(g_Model));
    getCmdLineEntry("camera", g_Camera, sizeof(g_Camera));

    if (!strcmp(g_Manufacturer, "coby")) {
        // For all Coby models, "utmodel" entry may be absent, and "ts"
        // may be present in it's place.
        if (strlen(g_Model) == 0)
            getCmdLineEntry("ts", g_Model, sizeof(g_Model));

        // For 1024N, make some modifications to the model.
        if (!strcmp(g_Model, "1024n"))
            strcpy(g_Model, "1024");

        // If we are still unable to detect the model, detect from LCD.
        if (strlen(g_Model) == 0) {
            if (!strcmp(g_LCD, "ut7gm"))
                strcpy(g_Model, "7024");
            else if (!strcmp(g_LCD, "ut08gm"))
                strcpy(g_Model, "8024");
            else if (!strcmp(g_LCD, "lp101"))
                strcpy(g_Model, "1024");
            else
                printk("*** WARNING cannot determine Coby model ***\n");
        }
    }

    printk("Got lcd=%s, utmodel=%s, camera=%s, man=%s...\n", g_LCD, g_Model, g_Camera, g_Manufacturer);
}

