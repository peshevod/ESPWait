
#ifndef COMPONENTS_S2LP_CONSOLE_S2LP_CONSOLE_H_
#define COMPONENTS_S2LP_CONSOLE_S2LP_CONSOLE_H_

typedef enum {
	SERIAL_CONSOLE = 0x00,
	BT_CONSOLE = 0x01
} console_type;


void start_s2lp_console(void);

#endif /* COMPONENTS_S2LP_CONSOLE_S2LP_CONSOLE_H_ */
