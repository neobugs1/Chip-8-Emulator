#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include "SDL.h"

// SDL Container
typedef struct
{
	SDL_Window *window;
	SDL_Renderer *renderer;
} sdl_t;

// EMU CONFIG
typedef struct
{
	uint32_t window_width;	// SDL window width
	uint32_t window_height; // SDL window height
	uint32_t fg_color;		// Foreground color RGBA8888
	uint32_t bg_color;		// Background color RGBA8888
	uint32_t scale_factor;	// Amount to scale up the screen (multiplication)
} config_t;

// EMU STATES
typedef enum
{
	QUIT,
	RUNNING,
	PAUSED,
} emulator_state_t;

typedef struct
{
	uint16_t opcode;
	uint16_t NNN; // 12 bit адреса/константа
	uint8_t NN;	  // 8 bit константа
	uint8_t N;	  // 4 bit константа
	uint8_t X;	  // 4 bit идентификатор за регистер
	uint8_t Y;	  // 4 bit идентификатор за регистер
} instruction_t;

// CHIP8 Machine Object
typedef struct
{
	uint8_t ram[4096];
	emulator_state_t state;
	bool display[64 * 32]; // емулирај пиксели на оригинална Chip8 резолуција
	uint16_t stack[12];	   // Subroutine stack // субрутина е сет од инструкции наменети да извршуваат често користени операции во програма
	uint16_t *stack_ptr;   // stack pointer
	uint8_t V[16];		   // Data registers V0-VF
	uint16_t I;			   // Index register;
	uint16_t PC;		   // Program Counter
	uint8_t delay_timer;   // Decrements at 60hz when >0
	uint8_t sound_timer;   // Decrements at 60hz and plays tone when >0
	bool keypad[16];	   // Hexadecimal keypad 0x0-0xF
	char *rom_name;		   // Currently running ROM
	instruction_t inst;	   // Currently executing instruction
} chip8_t;

// init sdl
bool init_sdl(sdl_t *sdl, const config_t config)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
	{
		SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
		return false;
	}
	sdl->window = SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);

	if (!sdl->window)
	{
		SDL_Log("Could not create window %s\n", SDL_GetError());
		return false;
	}

	sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);

	if (!sdl->renderer)
	{
		SDL_Log("Could not create Renderer %s\n", SDL_GetError());
		return false;
	}

	return true; // Success
}

// Set up initial emulator configuration from passed in arguments
bool set_config_from_args(config_t *config, const int argc, char **argv)
{
	// Defaults
	*config = (config_t){
		.window_width = 64,		// Chip8 original X resolution
		.window_height = 32,	// Chip8 original Y resolution
		.fg_color = 0xFFFF00FF, // YELLER
		.bg_color = 0x00000000, // BLACK
		.scale_factor = 20,		// Scale 64x32 by multiplying times 20
	};

	for (int i = 1; i < argc; i++)
	{
		// ...
		(void)argv[i];
	}
	return true; // Success
}

// INIT Chip8 machine
bool init_chip8(chip8_t *chip8, char rom_name[])
{
	const uint32_t entry_point = 0x200; // Chip8 Roms will be loaded to 0x200 aka memory location 512
	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80, // F
	};

	// Load font
	memcpy(&chip8->ram[0], font, sizeof(font));

	// Open ROM File
	FILE *rom = fopen(rom_name, "rb");
	if (!rom)
	{
		SDL_Log("Rom file %s is invalid or doesn't exist\n", rom_name);
		return false;
	}
	fseek(rom, 0, SEEK_END);
	const size_t rom_size = ftell(rom);
	const size_t max_size = sizeof chip8->ram - entry_point;
	rewind(rom);

	if (rom_size > max_size)
	{
		SDL_Log("Rom file is too big, max size allowed is %zu.\n", max_size);
		return false;
	}

	fread(&chip8->ram[entry_point], rom_size, 1, rom);

	fclose(rom);

	// Set chip8 machine defaul
	chip8->state = RUNNING; // DEFAULT STATE = RUNNING
	chip8->PC = entry_point;
	chip8->rom_name = rom_name;
	chip8->stack_ptr = &chip8->stack[0];

	return true;
}

// final cleanup
void final_cleanup(const sdl_t sdl)
{
	SDL_DestroyRenderer(sdl.renderer);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit();
}

// Init Screen Clear to background color
void clear_screen(const sdl_t sdl, const config_t config)
{
	const uint8_t r = (config.bg_color >> 24) & 0xFF; // maybe dont need 0xFF?
	const uint8_t g = (config.bg_color >> 16) & 0xFF;
	const uint8_t b = (config.bg_color >> 8) & 0xFF;
	const uint8_t a = (config.bg_color >> 0) & 0xFF;

	SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
	SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl)
{
	SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8)
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
		switch (event.type)
		{
		case SDL_QUIT:
			chip8->state = QUIT; // EXIT EMULATOR LOOP
			return;
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				chip8->state = QUIT;
				puts("==== EXIT BUTTON ====");
				break;
			case SDLK_SPACE:
				if (chip8->state == RUNNING)
				{
					chip8->state = PAUSED;
					printf("Enum value: %d\n", (int)chip8->state);
					puts("==== PAUSED ====");
				}
				else
				{
					chip8->state = RUNNING;
					puts("==== RESUME ====");
				}
				break;
			default:
				break;
			}
			break;
		case SDL_KEYUP:

			break;
		default:
			break;
		}
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8)
{
	printf("Address: 0x%04X, Opcode: 0x%04X Desc:", chip8->PC - 2, chip8->inst.opcode);
	switch ((chip8->inst.opcode >> 12) & 0x0F)
	{
	case 0x00:
		if (chip8->inst.NN == 0xE0)
		{
			// 0x00E0: Clear the screen
			memset(&chip8->display[0], false, sizeof chip8->display);
			printf("Clean screen\n");
		}
		else if (chip8->inst.NN == 0xEE)
		{
			// 0x00EE: Return from subroutine
			// Set program counter to last addres on subroutine stack so that next opcode will be gotten from that address
			printf("Return from subroutine to address 0x%04X\n", *(chip8->stack_ptr - 1));
			chip8->PC = *--chip8->stack_ptr;
		}
		break;
	case 0x02:
		//
		*chip8->stack_ptr++ = chip8->PC; // Store current address to return to on subroutine stack
		chip8->PC = chip8->inst.NNN;	 // set PC to subroutine address so that the next opcode is gotten from there.
		break;
	default:
		printf("Unimplemented\n");
		break;
	}
}
#endif

void emulate_instruction(chip8_t *chip8, const config_t config)
{
	chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1]; // следен operation code од рам
	chip8->PC += 2;																   // инкрементирање на Program Counter за 2 бајти затоа што 1 опкод е 16 бита

	// Current instruction format
	chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
	chip8->inst.NN = chip8->inst.opcode & 0x0FF;
	chip8->inst.N = chip8->inst.opcode & 0x0F;
	chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
	chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
	print_debug_info(chip8);
#endif

	// Emulate opcode
	switch ((chip8->inst.opcode >> 12) & 0x0F)
	{
	case 0x00:
	{
		if (chip8->inst.NN == 0xE0)
		{
			// 0x00E0: Clear the screen
			memset(&chip8->display[0], false, sizeof chip8->display);
		}
		else if (chip8->inst.NN == 0xEE)
		{
			// 0x00EE: Return from subroutine
			// Set program counter to last addres on subroutine stack so that next opcode will be gotten from that address
			chip8->PC = *--chip8->stack_ptr;
		}
		break;
	}
	case 0x02:
	{
		//
		*chip8->stack_ptr++ = chip8->PC; // Store current address to return to on subroutine stack
		chip8->PC = chip8->inst.NNN;	 // set PC to subroutine address so that the next opcode is gotten from there.
		break;
	}

	case 0x06:
	{
		// 0x6XNN: Set register VX to NN
		printf("Set register V%X to NN (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
		chip8->V[chip8->inst.X] = chip8->inst.NN;
	}

	case 0x0A:
	{
		// 0xANNN: SET index register I to NNN
		printf("SET index register I to NNN (0x%04X)\n", chip8->inst.NNN);
		chip8->I = chip8->inst.NNN;
		break;
	}

	case 0x0D:
	{
		// 0xDXYN: Draw N-height sprite at coords X,Y; Read from mem location I;
		// Screen pixels are XOR'd with sprite bits,
		// VF carry flag is set any screen pixles are set off; This is useful for collision detection or other reasons
		const uint8_t X = chip8->V[chip8->inst.X] % config.window_width;
		const uint8_t Y = chip8->V[chip8->inst.Y] % config.window_height;
	}

	default:
		break;
	}
}

int main(int argc, char **argv)
{
	// Default Usage message for args
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	puts("forsen");

	// Init emulator configurations/options
	config_t config = {0};
	if (!set_config_from_args(&config, argc, argv))
		exit(EXIT_FAILURE);

	// Иницијализација на SDL2
	sdl_t sdl;
	if (!init_sdl(&sdl, config))
		exit(EXIT_FAILURE);

	// Иницијализација на CHIP8
	chip8_t chip8;
	char *rom_name = argv[1];
	if (!init_chip8(&chip8, rom_name))
		exit(EXIT_FAILURE);

	// Init Screen Clear to background color
	clear_screen(sdl, config);

	// Main Emulator loop
	while (chip8.state != QUIT)
	{
		// Handle input
		handle_input(&chip8);

		if (chip8.state == PAUSED)
			continue;

		// Emulate
		emulate_instruction(&chip8, config);

		// Delay for 60hz
		SDL_Delay(16);

		// update Window
		update_screen(sdl);
	}

	// Final Cleanup
	final_cleanup(sdl);

	exit(EXIT_SUCCESS);
}