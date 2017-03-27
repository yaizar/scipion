
//Host includes
#include "cuda_gpu_rotate_image_v2.h"
#include <iostream>
#include <stdio.h>
#include <math.h>
//CUDA includes
#include <cuda_runtime.h>

#define Pole (sqrt(3.0f)-2.0f)  //pole for cubic b-spline

// 2D float texture
texture<float, 2, cudaReadModeElementType> texRef;


//CUDA functions

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef signed char schar;

inline __device__ __host__ uint UMIN(uint a, uint b)
{
	return a < b ? a : b;
}

inline __device__ __host__ uint PowTwoDivider(uint n)
{
	if (n == 0) return 0;
	uint divider = 1;
	while ((n & divider) == 0) divider <<= 1;
	return divider;
}

inline __host__ __device__ float2 operator-(float a, float2 b)
{
	return make_float2(a - b.x, a - b.y);
}

inline __host__ __device__ float3 operator-(float a, float3 b)
{
	return make_float3(a - b.x, a - b.y, a - b.z);
}






cudaPitchedPtr CopyVolumeHostToDevice(const float* host, uint width, uint height, uint depth)
{
	cudaPitchedPtr device = {0};
	const cudaExtent extent = make_cudaExtent(width * sizeof(float), height, depth);
	cudaMalloc3D(&device, extent);
	cudaMemcpy3DParms p = {0};
	p.srcPtr = make_cudaPitchedPtr((void*)host, width * sizeof(float), width, height);
	p.dstPtr = device;
	p.extent = extent;
	p.kind = cudaMemcpyHostToDevice;
	cudaMemcpy3D(&p);
	return device;
}



template<class floatN>
__host__ __device__ floatN InitialCausalCoefficient(
	floatN* c,			// coefficients
	uint DataLength,	// number of coefficients
	int step)			// element interleave in bytes
{
	const uint Horizon = UMIN(12, DataLength);

	// this initialization corresponds to clamping boundaries
	// accelerated loop
	float zn = Pole;
	floatN Sum = *c;
	for (uint n = 0; n < Horizon; n++) {
		Sum += zn * *c;
		zn *= Pole;
		c = (floatN*)((uchar*)c + step);
	}
	return(Sum);
}

template<class floatN>
__host__ __device__ floatN InitialAntiCausalCoefficient(
	floatN* c,			// last coefficient
	uint DataLength,	// number of samples or coefficients
	int step)			// element interleave in bytes
{
	// this initialization corresponds to clamping boundaries
	return((Pole / (Pole - 1.0f)) * *c);
}

template<class floatN>
__host__ __device__ void ConvertToInterpolationCoefficients(
	floatN* coeffs,		// input samples --> output coefficients
	uint DataLength,	// number of samples or coefficients
	int step)			// element interleave in bytes
{
	// compute the overall gain
	const float Lambda = (1.0f - Pole) * (1.0f - 1.0f / Pole);

	// causal initialization
	floatN* c = coeffs;
	floatN previous_c;  //cache the previously calculated c rather than look it up again (faster!)
	*c = previous_c = Lambda * InitialCausalCoefficient(c, DataLength, step);
	// causal recursion
	for (uint n = 1; n < DataLength; n++) {
		c = (floatN*)((uchar*)c + step);
		*c = previous_c = Lambda * *c + Pole * previous_c;
	}
	// anticausal initialization
	*c = previous_c = InitialAntiCausalCoefficient(c, DataLength, step);
	// anticausal recursion
	for (int n = DataLength - 2; 0 <= n; n--) {
		c = (floatN*)((uchar*)c - step);
		*c = previous_c = Pole * (previous_c - *c);
	}
}





template<class floatN>
__global__ void SamplesToCoefficients2DX(
	floatN* image,		// in-place processing
	uint pitch,			// width in bytes
	uint width,			// width of the image
	uint height)		// height of the image
{
	// process lines in x-direction
	const uint y = blockIdx.x * blockDim.x + threadIdx.x;
	floatN* line = (floatN*)((uchar*)image + y * pitch);  //direct access

	ConvertToInterpolationCoefficients(line, width, sizeof(floatN));
}

template<class floatN>
__global__ void SamplesToCoefficients2DY(
	floatN* image,		// in-place processing
	uint pitch,			// width in bytes
	uint width,			// width of the image
	uint height)		// height of the image
{
	// process lines in x-direction
	const uint x = blockIdx.x * blockDim.x + threadIdx.x;
	floatN* line = image + x;  //direct access

	ConvertToInterpolationCoefficients(line, height, pitch);
}


template<class floatN>
extern void CubicBSplinePrefilter2DTimer(floatN* image, uint pitch, uint width, uint height)
{

	dim3 dimBlockX(min(PowTwoDivider(height), 64));
	dim3 dimGridX(height / dimBlockX.x);
	SamplesToCoefficients2DX<floatN><<<dimGridX, dimBlockX>>>(image, pitch, width, height);

	dim3 dimBlockY(min(PowTwoDivider(width), 64));
	dim3 dimGridY(width / dimBlockY.x);
	SamplesToCoefficients2DY<floatN><<<dimGridY, dimBlockY>>>(image, pitch, width, height);

}





template<class T> inline __device__ void bspline_weights(T fraction, T& w0, T& w1, T& w2, T& w3)
{
	const T one_frac = 1.0f - fraction;
	const T squared = fraction * fraction;
	const T one_sqd = one_frac * one_frac;

	w0 = 1.0f/6.0f * one_sqd * one_frac;
	w1 = 2.0f/3.0f - 0.5f * squared * (2.0f-fraction);
	w2 = 2.0f/3.0f - 0.5f * one_sqd * (2.0f-one_frac);
	w3 = 1.0f/6.0f * squared * fraction;
}

template<class floatN, class T>
__device__ floatN cubicTex2D(texture<T, 2, mode> tex, float x, float y)
{
	// transform the coordinate from [0,extent] to [-0.5, extent-0.5]
	const float2 coord_grid = make_float2(x - 0.5f, y - 0.5f);
	const float2 index = floor(coord_grid);
	const float2 fraction = coord_grid - index;
	float2 w0, w1, w2, w3;
	bspline_weights(fraction, w0, w1, w2, w3);

	const float2 g0 = w0 + w1;
	const float2 g1 = w2 + w3;
	const float2 h0 = (w1 / g0) - make_float2(0.5f) + index;  //h0 = w1/g0 - 1, move from [-0.5, extent-0.5] to [0, extent]
	const float2 h1 = (w3 / g1) + make_float2(1.5f) + index;  //h1 = w3/g1 + 1, move from [-0.5, extent-0.5] to [0, extent]

	// fetch the four linear interpolations
	floatN tex00 = tex2D(tex, h0.x, h0.y);
	floatN tex10 = tex2D(tex, h1.x, h0.y);
	floatN tex01 = tex2D(tex, h0.x, h1.y);
	floatN tex11 = tex2D(tex, h1.x, h1.y);

	// weigh along the y-direction
	tex00 = g0.y * tex00 + g1.y * tex01;
	tex10 = g0.y * tex10 + g1.y * tex11;

	// weigh along the x-direction
	return (g0.x * tex00 + g1.x * tex10);
}


__global__ void
interpolate_kernel(float* output, uint width, float2 extent, float2 a)
{
	uint x = __umul24(blockIdx.x, blockDim.x) + threadIdx.x;
	uint y = __umul24(blockIdx.y, blockDim.y) + threadIdx.y;
	uint i = __umul24(y, width) + x;

	float x0 = (float)x;
	float y0 = (float)y;
	float x1 = a.x * x0 - a.y * y0 + shift.x;
	float y1 = a.x * y0 + a.y * x0 + shift.y;

	bool inside =
		-0.5f < x1 && x1 < (extent.x - 0.5f) &&
		-0.5f < y1 && y1 < (extent.y - 0.5f);

	output[i] = cubicTex2D(textRef, x1, y1);

}


cudaPitchedPtr interpolate(
	uint width, uint height, double angle)
{
	// Prepare the geometry
	float2 a = make_float2((float)cos(angle), (float)sin(angle));

	// Allocate the output image
	float* output;
	cudaMalloc((void**)&output, width * height * sizeof(float));

	// Visit all pixels of the output image and assign their value
	dim3 blockSize(min(PowTwoDivider(width), 16), min(PowTwoDivider(height), 16));
	dim3 gridSize(width / blockSize.x, height / blockSize.y);
	float2 extent = make_float2((float)width, (float)height);
	interpolate_kernel<<<gridSize, blockSize>>>(output, width, extent, a);

	return make_cudaPitchedPtr(output, width * sizeof(float), width, height);
}



void CopyVolumeDeviceToHost(float* host, const cudaPitchedPtr device, uint width, uint height, uint depth)
{
	const cudaExtent extent = make_cudaExtent(width * sizeof(float), height, depth);
	cudaMemcpy3DParms p = {0};
	p.srcPtr = device;
	p.dstPtr = make_cudaPitchedPtr((void*)host, width * sizeof(float), width, height);
	p.extent = extent;
	p.kind = cudaMemcpyDeviceToHost;
	cudaMemcpy3D(&p);
	cudaFree(device.ptr);  //free the GPU volume
}



void cuda_rotate_image_v2(float *image, float *rotated_image, size_t Xdim, size_t Ydim, float ang){

	std::cerr  << "Inside CUDA function " << ang << std::endl;

	//CUDA code
	size_t matSize=Xdim*Ydim*sizeof(float);

	//Filtering process (first step)
	struct cudaPitchedPtr bsplineCoeffs, cudaOutput;
	bsplineCoeffs = CopyVolumeHostToDevice(image, (uint)Xdim, (uint)Ydim, 1);
	CubicBSplinePrefilter2DTimer((float*)bsplineCoeffs.ptr, (uint)bsplineCoeffs.pitch, (uint)Xdim, (uint)Ydim);

	// Init texture
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<float>();
    cudaArray* cuArray;
    cudaMallocArray(&cuArray, &channelDesc, Xdim, Ydim);
    // Copy to device memory some data located at address h_data in host memory
    cudaMemcpyToArray(cuArray, 0, 0, bsplineCoeffs.ptr, bsplineCoeffs.pitch, Xdim * sizeof(float), Ydim, cudaMemcpyHostToDevice);

    // Bind the array to the texture reference
    cudaBindTextureToArray(texRef, cuArray, channelDesc);

    // Specify texture object parameters
    texRef.filterMode = cudaFilterModeLinear;
    texRef.normalized = false;

    //Interpolation (second step)
    cudaOutput = interpolate(Xdim, Ydim, ang);

    float *d_output;
    cudaMalloc((void **)&d_output, matSize);
    CopyVolumeDeviceToHost(d_output, cudaOutput, Xdim, Ydim, 1);

    cudaMemcpy(rotated_image, d_output, matSize, cudaMemcpyDeviceToHost);

    cudaFree(cuArray);
    cudaFree(d_output);


}
