/*
 * compile both driver code and kernel code with nvcc, as in:
 * 			nvcc simple.c simple.cu
 */
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <ctime>
using namespace std;

extern "C" void gpu_sobel(int **source_array, int **result_array, int src_rows,
                          int src_column_size);

#pragma pack(1)
typedef struct {
  char id[2];
  int file_size;
  int reserved;
  int offset;
} header_type;

#pragma pack(1)
typedef struct {
  int header_size;
  int width;
  int height;
  unsigned short int color_planes;
  unsigned short int color_depth;
  unsigned int compression;
  int image_size;
  int xresolution;
  int yresolution;
  int num_colors;
  int num_important_colors;
} information_type;

int main(int argc, char *argv[]) {
  header_type header;
  information_type information;
  string imageFileName, newImageFileName;
  unsigned char tempData[3];
  int row, col, row_bytes, padding;

  // prepare files
  // cout << "Original imagefile? ";
  imageFileName = argv[1];
  ifstream imageFile;
  imageFile.open(imageFileName.c_str(), ios::binary);
  if (!imageFile) {
    cerr << "file not found" << endl;
    exit(-1);
  }
  // std::cout << "New imagefile name? ";
  newImageFileName = argv[2];
  ofstream newImageFile;
  newImageFile.open(newImageFileName.c_str(), ios::binary);

  // read file header
  imageFile.read((char *)&header, sizeof(header_type));
  if (header.id[0] != 'B' || header.id[1] != 'M') {
    cerr << "Does not appear to be a .bmp file.  Goodbye." << endl;
    exit(-1);
  }
  // read/compute image information
  imageFile.read((char *)&information, sizeof(information_type));
  row_bytes = information.width * 3;
  padding = row_bytes % 4;
  if (padding)
    padding = 4 - padding;

  // extract image data, initialize vectors
  int rows = information.height + 2;
  int column_size = information.width + 2;
  int src_size = rows * column_size;
  int **data = (int **)malloc(src_size * sizeof(int *));
  for (row = 1; row <= information.height; row++) {
    data[row] = (int *)malloc(column_size * sizeof(int));
    for (col = 0; col <= information.width; col++) {
      if (col == 0) {
        data[row][0] = 0;
      } else {
        imageFile.read((char *)tempData, 3 * sizeof(unsigned char));
        data[row][col] = ((int)tempData[0]);
      }
    }
    // pad last column
    data[row][col + 1] = 0;
    if (padding)
      imageFile.read((char *)tempData, padding * sizeof(unsigned char));
  }

  // pad first & last row
  int last_row = rows - 1;
  data[0] = (int *)malloc(column_size * sizeof(int));
  data[last_row] = (int *)malloc(column_size * sizeof(int));
  for (col = 0; col < column_size; col++) {
    data[0][col] = 0;
    data[rows - 1][col] = 0;
  }
  std::cout << imageFileName << ": " << information.width << " x "
            << information.height << std::endl;

  int size = information.width * information.height;
  int **newData = (int **)malloc(size * sizeof(int *));
  clock_t gpu_start = clock();
  gpu_sobel(data, newData, rows, column_size);
  clock_t gpu_stop = clock();
  double elapsed_gpu = double(gpu_stop - gpu_start) / (CLOCKS_PER_SEC / 1000);

  std::cout << "GPU Time Taken (msec): " << elapsed_gpu << std::endl;
  // write header to new image file
  newImageFile.write((char *)&header, sizeof(header_type));
  newImageFile.write((char *)&information, sizeof(information_type));

  // write new image data to new image file
  for (row = 0; row < information.height; row++) {
    for (col = 0; col < information.width; col++) {
      tempData[0] = (unsigned char)newData[row][col];
      tempData[1] = (unsigned char)newData[row][col];
      tempData[2] = (unsigned char)newData[row][col];
      newImageFile.write((char *)tempData, 3 * sizeof(unsigned char));
    }
    if (padding) {
      tempData[0] = 0;
      tempData[1] = 0;
      tempData[2] = 0;
      newImageFile.write((char *)tempData, padding * sizeof(unsigned char));
    }
  }
  cout << newImageFileName << " done." << endl;
  imageFile.close();
  newImageFile.close();

  return 0;
}
