#include "lodepng.h"
#include "libimagequant.h"
#include <stdlib.h>
#include <stdio.h>
#include "dirent.h"
#include <Windows.h>

#define EXIT_NO_ERROR 2
#define EXIT_WRONG_NUM_ARGS 3

#define ERR_ARGS "Required argument: Directory path to images from BDEdit\nUse argument \"-help\" for more information."
#define ERR_LODEPNG lodepng_error_text(status)
#define ERR_LIBQUANT "Quantization failed."
#define ERR_FILE_OUT "Unable to write output file."

int error(int code, const char* msg);
char* pathcat(const char* str1, char* str2);

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		return error(EXIT_WRONG_NUM_ARGS, ERR_ARGS);
	}

	if (strcmp(argv[1], "-help") == 0)
	{
		printf("This tool takes in a directory of files from BDEdit and generates a palette for them, then quantizes a different directory of images with that palette.");
		printf("This tool is meant to be used for editing Interactive Graphics (IG) streams used in BluRay menus.");
		printf("\n\nUSAGE:\n1) Save out all of the images in an IG menu using BDEdit.\n2) Modify them as needed.\n");
		printf("3) Use this tool; output is into a \"new\" folder in the input directory.");
		printf("\n4) Go back to BDEdit and replace the ALL images, being sure to check both checkboxes to replace the palette and object.\n\n");
		return error(EXIT_NO_ERROR, ERR_ARGS);
	}

	if (argv[1][strlen(argv[1]) - 1] != '\\')
	{
		argv[1] = pathcat(argv[1], "\\");
	}
	char *mod_dir = pathcat(argv[1], "new\\");
	CreateDirectory(mod_dir, NULL);

	DIR *source_dir = opendir(argv[1]);
	liq_attr *handle = liq_attr_create();
	liq_set_last_index_transparent(handle, 1);
	liq_set_speed(handle, 1);
	liq_histogram *histogram = liq_histogram_create(handle);
	dirent *source_file_entry;

	while (source_file_entry = readdir(source_dir))
	{
		char* source_file_name = pathcat(argv[1], source_file_entry->d_name);
		if (!strstr(source_file_name, ".png"))
		{
			continue;
		}

		unsigned char *raw_rgba_pixels_source;
		unsigned int source_width, source_height;
		unsigned int status = lodepng_decode32_file(&raw_rgba_pixels_source, &source_width, &source_height, source_file_name);
		if (status)
		{
			return error(EXIT_FAILURE, ERR_LODEPNG);
		}

		liq_image* source_image = liq_image_create_rgba(handle, raw_rgba_pixels_source, source_width, source_height, 0);
		liq_histogram_add_image(histogram, handle, source_image);

		liq_image_destroy(source_image);
		free(source_file_name);
	}

	liq_result *quant_result;
	liq_error err = liq_histogram_quantize(histogram, handle, &quant_result);
	if (LIQ_OK != err)
	{
		return error(EXIT_FAILURE, ERR_LIBQUANT);
	}
	liq_set_dithering_level(quant_result, 0.0);
	const liq_palette *palette = liq_get_palette(quant_result);
	liq_histogram_destroy(histogram);

	rewinddir(source_dir);
	while (source_file_entry = readdir(source_dir))
	{
		char* source_file_name = pathcat(argv[1], source_file_entry->d_name);
		if (!strstr(source_file_name, ".png"))
		{
			continue;
		}

		unsigned char *raw_rgba_pixels_source;
		unsigned int source_width, source_height;
		unsigned int status = lodepng_decode32_file(&raw_rgba_pixels_source, &source_width, &source_height, source_file_name);
		if (status)
		{
			return error(EXIT_FAILURE, ERR_LODEPNG);
		}

		liq_image* source_image = liq_image_create_rgba(handle, raw_rgba_pixels_source, source_width, source_height, 0);

		size_t image_size = source_width*source_height;
		unsigned char *raw_8bit_pixels_mod = (unsigned char*) malloc(image_size);
		liq_write_remapped_image(quant_result, source_image, raw_8bit_pixels_mod, image_size);
		liq_image_destroy(source_image);

		LodePNGState state_out;
		lodepng_state_init(&state_out);
		state_out.info_raw.colortype = LCT_PALETTE;
		state_out.info_raw.bitdepth = 8;
		state_out.info_png.color.colortype = LCT_PALETTE;
		state_out.info_png.color.bitdepth = 8;

		for (unsigned int i = 0; i < palette->count; i++) {
			lodepng_palette_add(&state_out.info_png.color, palette->entries[i].r, palette->entries[i].g, palette->entries[i].b, palette->entries[i].a);
			lodepng_palette_add(&state_out.info_raw, palette->entries[i].r, palette->entries[i].g, palette->entries[i].b, palette->entries[i].a);
		}

		unsigned char *output_file_data;
		size_t output_file_size;
		status = lodepng_encode(&output_file_data, &output_file_size, raw_8bit_pixels_mod, source_width, source_height, &state_out);
		if (status) {
			return error(EXIT_FAILURE, ERR_LODEPNG);
		}

		char* mod_file_name = pathcat(mod_dir, source_file_entry->d_name);
		FILE *fp = fopen(mod_file_name, "wb");
		if (!fp) {
			return error(EXIT_FAILURE, ERR_FILE_OUT);
		}
		fwrite(output_file_data, 1, output_file_size, fp);
		fclose(fp);

		free(raw_8bit_pixels_mod);
		lodepng_state_cleanup(&state_out);
	}

	liq_result_destroy(quant_result);
	liq_attr_destroy(handle);
	printf("Done!");
}

int error(int code, const char *msg) 
{
	printf("%s", msg);
	return code;
}

char* pathcat(const char *str1, char *str2)
{
	char *res;
	size_t strlen1 = strlen(str1);
	size_t strlen2 = strlen(str2);
	res = (char*)malloc((strlen1 + strlen2 + 1)*sizeof *res);
	strcpy(res, str1);
	for (unsigned int i = strlen1, j = 0; ((i<(strlen1 + strlen2)) && (j<strlen2)); i++, j++)
		res[i] = str2[j];
	res[strlen1 + strlen2] = '\0';
	return res;
}