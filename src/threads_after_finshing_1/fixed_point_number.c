#include "threads/fixed_point_number.h"

/* x and y represent float numbers while n represents an integer.
*/

int convert_to_fixed_point(int n)
{
	return n<<FIXED_CONSTANT;
}
int convert_to_int(int x)
{
	return x>>FIXED_CONSTANT;
}
int multiply_float(int x, int y)
{
	return ((int64_t)(x)* y ) >> FIXED_CONSTANT;
}
int multiply_int(int x, int n)
{
	return x*n;
}
int add_int(int x, int n)
{
	return x + (n<<FIXED_CONSTANT);
}
int add_float(int x, int y)
{
	return x+y;
}
int sub_int(int x, int n)
{
	return x - (n<<FIXED_CONSTANT);
}
int sub_float(int x, int y)
{
	return x-y;
}

int divide_float(int x, int y)
{
	return ((int64_t)(x) << FIXED_CONSTANT) / y;
}
int divide_int(int x, int n)
{
	return x / n;
}
