
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string>

//   |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
char raster[] =
    "         #    # #  # #   ###  #   #   ##     #    ##   ##   # # #   #                         #  ###     #   ###  ####     #  #####   ##  #####  ###   ###                  #       #      ###   ###   ###  ####   #### ####  ##### #####  #### #   #  ####     # #   # #     #   # #   #  ###  ####   ###  ####   #### ##### #   # #   # #   # #   # #   # ##### "\
    "         #    # # ##### # #      #   #       #   #       #   ###    #                        #  #   #   ##  #   #     #   ##  #      #        # #   # #   #    #     #    ##  #####  ##       # # ### #   # #   # #     #   # #     #     #     #   #   #       # #  #  #     ## ## ##  # #   # #   # #   # #   # #       #   #   # #   # #   #  # #   # #     #  "\
    "         #         # #   ###    #     # #        #       #  ##### #####        ###          #   # # #    #     #   ###   # #  ####  ####   ###   ###   ####             ##            ##    ##  # # # ##### ####  #     #   # ###   ###   #  ## #####   #   #   # ###   #     # # # # # # #   # ####  # # # ####   ###    #   #   # #   # # # #   #     #     #   "\
    "                  #####   # #  #     # #         #       #   ###    #                      #    #   #    #    #       # #####     # #   #   #   #   #     #    #     #    ##  #####  ##         # ### #   # #   # #     #   # #     #     #   # #   #   #   #   # #  #  #     #   # #  ## #   # #     #  #  #   #     #   #   #   #  # #  ## ##  # #    #    #    "\
    "         #         # #   ###  #   #   # #         ##   ##   # # #   #      #          #   #      ###    ### ##### ####     #  ####   ###   #     ###   ###          #       #       #       #    ###  #   # #####  #### ####  ##### #      ###  #   #  ####  ###  #   # ##### #   # #   #  ###  #      ## # #   # ####    #    ###    #    # #  #   #   #   ##### "\
    "                                                                          #                                                                                                                                                                                                                                                                                       ";
//   000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000011111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111112222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222333333333333333333333333333333333333333333333333333333333333
//   000000000011111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999999990000000000111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999000000000011111111112222222222333333333344444444445555555555
//   012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789

int main (int argc, char * argv[])
{
	char teststring[] = "1234";
	int const char_width = 6;
	int const char_height = 6;
	int const canvas_width = 40;
	int const canvas_height = 6;
	char canvas[canvas_width*canvas_height];
	int const raster_chars = 59;
	int const raster_width = char_width * raster_chars;
	int const raster_height = char_height;
	int const raster_sizeof = raster_height * raster_width;

	int i=0;
  	while (teststring[i])
  	{
    	teststring[i] = toupper(teststring[i]);
    	i++;
  	}


	for (int offset = -canvas_width; offset < canvas_width; offset++)
	{
		for (unsigned int lin = 0; lin < canvas_height; lin++)
		{
			for (unsigned int col = 0+offset; col < canvas_width+offset; col++)
			{
				int teststring_index = col / char_width;
				//printf("%i   ", teststring_index);
				teststring_index %= sizeof(teststring);
				int character_to_print_ascii = teststring[teststring_index];
				int character_to_print_rasterindex = character_to_print_ascii - 32;
				character_to_print_rasterindex %= raster_chars;
				int dot_to_print_index = character_to_print_rasterindex * char_width + col%char_width + lin * raster_width;
				if (dot_to_print_index < 0) dot_to_print_index = 0;
				if (dot_to_print_index >= raster_sizeof -1 ) dot_to_print_index = raster_sizeof -1;
				int dot_to_print = raster[dot_to_print_index];
				printf("%c", (char) dot_to_print);
			}
			printf("\n");
		}
	}
	for (unsigned int i = 0; i < strlen(teststring); i++)
	{
		for (unsigned int j = 0; j<6; j++)
		{
			for (unsigned int k = 0; k<6; k++)
			{

			}
		}

	}
	system("pause");
	return 0;

}
