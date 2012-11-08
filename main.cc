/*
* Motorola SREC to Xilinx COE Converter
*
* Phillip Johnston
* 27 October 2012
*/

#include <cstdlib>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>

//#define DEBUG_M

using namespace std;

/*
* Function Declarations
*/ 
void displayCLIError();
void displayHelpMenu();
void checkConvertRC(int rc);
int convert(string f_in, string f_out);
uint8_t convertFromASCIItoHex(char val);
uint8_t getHeaderInfo(char * buffer, uint16_t * address, uint8_t * line_type);
void setupCoe(fstream & file, string input);
void writeData(char * input, fstream & file, uint8_t size);

/*
* Main.c
*/

int main(int argc, char * argv[])
{	
	if(argc == 2)
	{
		string arg_val(argv[1]);
		
		if(!arg_val.compare("--help"))
		{
			displayHelpMenu();
			exit(0);
		}
		else
		{
			//Treat it as the input filename.
			int rc = convert(arg_val, string());
			checkConvertRC(rc);
		}
	}
	else if(argc == 4)
	{
		string infile(argv[1]), tag(argv[2]), outfile(argv[3]);
		if(!tag.compare("-out"))
		{
			int rc = convert(infile, outfile);
			checkConvertRC(rc);
		}
		else
		{
			displayCLIError();
		}
	}
	else
	{
		displayCLIError();
	}

	return 0;
}

void displayHelpMenu()
{
	cout << "Help for mtoc" << endl << endl;
	cout << "mtoc will convert Motorola SREC hex files into valid Xilinx COE files for memory initialization" << endl << endl;
	cout << "mtoc will take in a .hex file and output a .coe file." << endl;
	
	cout << "Usage:  mtoc [options] <filename> [-out <filename2>]" << endl;
	
	cout << endl << "Options:" << endl;
	cout << "\t--help\t\tDisplays this information." << endl;
	cout << "\t-out\t\tExplicitly specify the name of the output file." << endl;
}

void displayCLIError()
{
	cout << "Simple usage is mtoc <infile>" << endl;
	cout << "For more information use \'mtoc --help\'" << endl;
	//fflush(stdout);
	exit(0);
}


int convert(string f_in, string f_out)
{
	char inbuf[256];
	if(f_out.empty())
	{
		//An output file name wasn't specified
		size_t fname_size = f_in.find(".", 0);
		f_out = f_in.substr(0, fname_size);
		f_out.append(".coe");
		#ifdef DEBUG_M
		cout << "Calculated f_out to be " << f_out << endl;
		#endif
	}
	
	fstream srec(f_in.c_str(), fstream::in);
	fstream coe(f_out.c_str(), fstream::out);
	
	setupCoe(coe, f_in);
	uint16_t curr_addr = 0;
	
	//Begin processing
	while(!srec.eof())
	{
		srec.getline((char *)&inbuf, 256);
		if(srec.eof())
		{
			break;
		}
		
		if(!strncmp(&inbuf[0], "S", 1))
		{
			uint16_t address;
			uint8_t line_type;
			uint8_t data_sz = getHeaderInfo((char *)&inbuf, &address, &line_type);
			uint8_t * outbuf = new uint8_t[data_sz];
			//translateData((char *)&inbuf, outbuf, data_sz);
			if(curr_addr < address)
			{
				//fill with zeroes
				for(int i = address; i < curr_addr; i++)
				{
					coe << "00," << endl;
				}
			}
			
			if(line_type == 1)
			{
				writeData((char *)&inbuf, coe, data_sz);
				curr_addr += data_sz;
			}
		}
		//otherwise move on
	}
	
	//For now, the easiest thing is to jsut write an extra byte of 0
	//to close the COE out.
	coe << "00;" << endl;
	
	srec.close();
	coe.close();

	return 0; //return successfully
}

void checkConvertRC(int rc)
{
	if(rc != 0)
	{
		cout << "Conversion of file failed!" << endl;
		#ifdef DEBUG_M
		cout << "Tell Phillip the error code that was returned: " << rc << endl;
		#warning "Fix this in case I ever publicly release"
		#endif
	}
	else
	{
		cout << "Conversion of file successful." << endl;
	}
}


uint8_t convertFromASCIItoHex(char val)
{
	if(val >= 48 && val <= 57)
	{
		//val is a number
		return val - 48;
	}
	else if(val >= 65 && val <= 70)
	{
		//A-F
		val = val - 65;
		return val + 10;
	}
	else
	{
		cout << "Invalid character detected!" << endl;
		fflush(stdout);
		assert(0);
		return 0;
	}
}

uint8_t getHeaderInfo(char * buffer, uint16_t * address, uint8_t * line_type)
{
	uint8_t num_bytes, curr_val;
	
	curr_val = convertFromASCIItoHex(buffer[1]);
	*line_type = curr_val;
	if(curr_val == 1)
	{
		//expected data sequence type
		curr_val = convertFromASCIItoHex(buffer[2]);
		num_bytes = curr_val << 4;
		curr_val = convertFromASCIItoHex(buffer[3]);
		num_bytes |= curr_val;
		num_bytes -= 3; //remove addr and checksum bytes from count
		
		//Get address
		curr_val = convertFromASCIItoHex(buffer[4]);
		*address = curr_val << 12;
		curr_val = convertFromASCIItoHex(buffer[5]);
		*address |= curr_val << 8;
		curr_val = convertFromASCIItoHex(buffer[6]);
		*address |= curr_val << 4;
		curr_val = convertFromASCIItoHex(buffer[7]);
		*address |= curr_val;
		
	}
	else if(curr_val == 9)
	{
		//starting address of program
		num_bytes = 0;
		
		//Get address
		curr_val = convertFromASCIItoHex(buffer[4]);
		*address = curr_val << 12;
		curr_val = convertFromASCIItoHex(buffer[5]);
		*address |= curr_val << 8;
		curr_val = convertFromASCIItoHex(buffer[6]);
		*address |= curr_val << 4;
		curr_val = convertFromASCIItoHex(buffer[7]);
		*address |= curr_val;
	}
	else
	{
		cout << "Code does not currently support SREC mode " << curr_val << endl;
		fflush(stdout);
		assert(0);
	}
	
	return num_bytes;
}


void setupCoe(fstream & file, string input)
{
	file << "; This program generated from " << input << endl;
	file << "; using mtoc (by Phillip Johnston)" << endl << endl;
	file << "memory_initialization_radix=16;" << endl;
	file << "memory_initialization_vector=" << endl;
	//file << "00," << endl << "00," << endl << "00," << endl << "00," << endl;
}

void writeData(char * input, fstream & file, uint8_t size)
{
	for(int i = 0; i < size * 2; i++)
	{
		if(i % 2 == 0)
		{
			file << input[8+i];
		}
		else
		{
			file << input[8+i] <<"," << endl;
		}
	}
}
