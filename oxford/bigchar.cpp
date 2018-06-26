
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

//   |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
char raster[] =
    " ###     #   ###  ####     #  #####   ##  #####  ###   ###                  #       #      ###   ###   ###  ####   #### ####  ##### #####  #### #   #  ####     # #   # #     #   # #   #  ###  ####   ###  ####   #### ##### #   # #   # #   # #   # #   # ##### "\
    "#   #   ##  #   #     #   ##  #      #        # #   # #   #    #     #    ##  #####  ##       # # ### #   # #   # #     #   # #     #     #     #   #   #       # #  #  #     ## ## ##  # #   # #   # #   # #   # #       #   #   # #   # #   #  # #   # #     #  "\
    "# # #    #     #   ###   # #  ####  ####   ###   ###   ####             ##            ##    ##  # # # ##### ####  #     #   # ###   ###   #  ## #####   #   #   # ###   #     # # # # # # #   # ####  # # # ####   ###    #   #   # #   # # # #   #     #     #   "\
    "#   #    #    #       # #####     # #   #   #   #   #     #    #     #    ##  #####  ##         # ### #   # #   # #     #   # #     #     #   # #   #   #   #   # #  #  #     #   # #  ## #   # #     #  #  #   #     #   #   #   #  # #  ## ##  # #    #    #    "\
    " ###    ### ##### ####     #  ####   ###   #     ###   ###          #       #       #       #    ###  #   # #####  #### ####  ##### #      ###  #   #  ####  ###  #   # ##### #   # #   #  ###  #      ## # #   # ####    #    ###    #    # #  #   #   #   ##### "\
    "                                                                                                                                                                                                                                                                  ";



int main (int argc, char * argv[])
{
	char teststring[] = "Print This string DUde";
	int const char_width = 6;
	int const char_height = 6;
	int const canvas_width = 20;
	int const canvas_height = 6;
	char canvas[canvas_width*canvas_height];
	int const raster_chars = 43;
	int const raster_width = char_width * raster_chars;
	int const raster_height = char_height;


	for (unsigned int offset = -canvas_width; offset < canvas_width; offset++)
	{

		for (unsigned int lin = 0; lin < canvas_height; lin++)
		{
			for (unsigned int col = 0; col < canvas_width; col++)
			{
				int teststring_index = col / char_width;
				int character_to_print = teststring[teststring_index];
				int raster_index = 
				int dot_to_print = raster[] 
				if (teststring[teststring_index]
					printf("@");
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
