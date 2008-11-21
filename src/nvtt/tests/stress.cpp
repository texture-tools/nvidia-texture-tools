// Copyright NVIDIA Corporation 2007 -- Ignacio Castano <icastano@nvidia.com>
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#include <nvtt/nvtt.h>
#include <nvimage/Image.h>
#include <nvimage/BlockDXT.h>
#include <nvimage/ColorBlock.h>
#include <nvcore/Ptr.h>
#include <nvcore/Debug.h>

#include <stdlib.h> // free
#include <string.h> // memcpy
#include <time.h> // clock

/*
#include <stdio.h> // printf
*/

using namespace nv;

static const char * s_fileNames[] = {
    "kodim01.png",
    "kodim02.png",
    "kodim03.png",
    "kodim04.png",
    "kodim05.png",
    "kodim06.png",
    "kodim07.png",
    "kodim08.png",
    "kodim09.png",
    "kodim10.png",
    "kodim11.png",
    "kodim12.png",
    "kodim13.png",
    "kodim14.png",
    "kodim15.png",
    "kodim16.png",
    "kodim17.png",
    "kodim18.png",
    "kodim19.png",
    "kodim20.png",
    "kodim21.png",
    "kodim22.png",
    "kodim23.png",
    "kodim24.png",
    "clegg.tif",
    "frymire.tif",
    "lena.tif",
    "monarch.tif",
    "sail.tif",
    "serrano.tif",
    "tulips.tif",
};
const int s_fileCount = sizeof(s_fileNames)/sizeof(s_fileNames[0]);


struct MyOutputHandler : public nvtt::OutputHandler
{
	MyOutputHandler() : m_data(NULL), m_ptr(NULL) {}
    ~MyOutputHandler()
    {
        free(m_data);
    }

	virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel)
	{
        m_size = size;
        m_width = width;
        m_height = height;
        free(m_data);
        m_data = (unsigned char *)malloc(size);
		m_ptr = m_data;
	}
	
	virtual bool writeData(const void * data, int size)
	{
		memcpy(m_ptr, data, size);
		m_ptr += size;
		return true;
	}

    Image * decompress(nvtt::Format format)
    {
        int bw = (m_width + 3) / 4;
        int bh = (m_width + 3) / 4;

        AutoPtr<Image> img( new Image() );
        img->allocate(m_width, m_height);

        if (format == nvtt::Format_BC1)
        {
            BlockDXT1 * block = (BlockDXT1 *)m_data;

            for (int y = 0; y < bh; y++)
            {
                for (int x = 0; x < bw; x++)
                {
                    ColorBlock colors;
                    block->decodeBlock(&colors);

                    for (int yy = 0; yy < 4; yy++)
                    {
                        for (int xx = 0; xx < 4; xx++)
                        {
                            Color32 c = colors.color(xx, yy);

                            if (x * 4 + xx < m_width && y * 4 + yy < m_height)
                            {
                                img->pixel(x * 4 + xx, y * 4 + yy) = c;
                            }
                        }
                    }

                    block++;
                }
            }
        }

        return img.release();
    }

    int m_size;
    int m_width;
    int m_height;
    unsigned char * m_data;
	unsigned char * m_ptr;
};


float rmsError(const Image * a, const Image * b)
{
    nvCheck(a != NULL);
    nvCheck(b != NULL);
    nvCheck(a->width() == b->width());
    nvCheck(a->height() == b->height());

    float mse = 0;

    const uint count = a->width() * a->height();

    for (uint i = 0; i < count; i++)
    {
        Color32 c0 = a->pixel(i);
        Color32 c1 = b->pixel(i);

        int r = c0.r - c1.r;
        int g = c0.g - c1.g;
        int b = c0.b - c1.b;
        //int a = c0.a - c1.a;

        mse += r * r;
        mse += g * g;
        mse += b * b;
    }

    mse /= count * 3;

    return sqrtf(mse);
}


int main(int argc, char *argv[])
{
    nvtt::InputOptions inputOptions;
    inputOptions.setMipmapGeneration(false);

    nvtt::CompressionOptions compressionOptions;
    compressionOptions.setFormat(nvtt::Format_BC1);
    compressionOptions.setQuality(nvtt::Quality_Production);

	nvtt::OutputOptions outputOptions;
    outputOptions.setOutputHeader(false);

	MyOutputHandler outputHandler;
	outputOptions.setOutputHandler(&outputHandler);

    nvtt::Compressor compressor;
	compressor.enableCudaAcceleration(false);

    float totalRMS = 0;

    for (int i = 0; i < s_fileCount; i++)
    {
        AutoPtr<Image> img( new Image() );
        
        if (!img->load(s_fileNames[i]))
        {
            printf("Input image '%s' not found.\n", s_fileNames[i]);
            return EXIT_FAILURE;
        }

        inputOptions.setTextureLayout(nvtt::TextureType_2D, img->width(), img->height());
        inputOptions.setMipmapData(img->pixels(), img->width(), img->height());

        printf("Compressing: '%s'\n", s_fileNames[i]);

		clock_t start = clock();

		compressor.process(inputOptions, compressionOptions, outputOptions);

		clock_t end = clock();
		printf("  Time taken: %.3f seconds\n", float(end-start) / CLOCKS_PER_SEC);

        AutoPtr<Image> img_out( outputHandler.decompress(nvtt::Format_BC1) );

        float rms = rmsError(img.ptr(), img_out.ptr());
        totalRMS += rms;

        printf("  RMS: %.4f\n", rms);
    }

    totalRMS /= s_fileCount;

    printf("Average Results:\n");
    printf("  RMS: %.4f\n", totalRMS);

	return EXIT_SUCCESS;
}

