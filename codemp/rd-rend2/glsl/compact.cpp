#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

typedef std::vector<std::string> string_list;

bool should_be_escaped ( char c )
{
	switch ( c )
	{
	case '\\':
	case '"':
		return true;
	default:
		return false;
	}
}

std::string& escape_string ( std::string& s )
{
	std::string::difference_type escapable_characters = std::count_if( s.begin(), s.end(), should_be_escaped );
	if ( escapable_characters == 0 )
	{
		return s;
	}

	if ( s.capacity() < s.length() + escapable_characters )
	{
		// Grow if necessary.
		s.resize (s.length() + escapable_characters);
	}

	std::string::iterator it = s.begin();
	while ( it != s.end() )
	{
		char c = *it;
		if ( should_be_escaped (c) )
		{
			it = s.insert (it, '\\');
			it += 2;
		}
		else
		{
			++it;
		}
	}

	return s;
}

bool ends_with ( const std::string& s, const std::string& suffix )
{
	return s.compare (s.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string program_name ( const std::string& shaderPath )
{
	std::string::size_type lastSlash = shaderPath.find_last_of ("\\/");
	std::string::size_type lastUnderscore = shaderPath.find_last_of ('_');

	return shaderPath.substr (lastSlash + 1, lastUnderscore - lastSlash - 1);
}

std::string shader_file ( const std::string& shaderPath )
{
	std::string::size_type lastSlash = shaderPath.find_last_of ("\\/");

	return shaderPath.substr (lastSlash + 1);
}

std::string shader_name ( const std::string& shaderPath )
{
	std::string::size_type lastSlash = shaderPath.find_last_of ("\\/");
	return shaderPath.substr (lastSlash + 1, shaderPath.length() - lastSlash - 6);
}

std::string shader_enum_type ( const std::string& shaderName )
{
	if ( ends_with (shaderName, "_fp") ) return "SHADERTYPE_FRAGMENT";
	if ( ends_with (shaderName, "_gp") ) return "SHADERTYPE_GEOMETRY";
	if ( ends_with (shaderName, "_vp") ) return "SHADERTYPE_VERTEX";

	return "";
}

int main ( int argc, char *argv[] )
{
	string_list args (argv, argv + argc);

	if ( args.empty() )
	{
		std::cerr << "No GLSL files were given.\n";
		return EXIT_FAILURE;
	}

	if ( args.size() < 3 )
	{
		// 0 = exe, 1 = outfile, 2+ = glsl files
		return EXIT_FAILURE;
	}

	std::string cppFile = args[1] + ".cpp";
	std::string headerFile = args[1] + ".h";
	string_list glslFiles (args.begin() + 2, args.end());

	std::sort (glslFiles.begin(), glslFiles.end());

	std::cout << "Outputting to " << cppFile << " and " << headerFile << '\n';

	std::ostringstream output;
	std::ostringstream headerOutput;

	output << "#include \"glsl_shaders.h\"\n\n";

	headerOutput << "#pragma once\n";
	headerOutput << "#include \"tr_local.h\"\n\n";

	std::string line;
	for ( string_list::const_iterator it = glslFiles.begin();
			it != glslFiles.end(); ++it )
	{
		// Get shader name from file name
		if ( !ends_with (*it, ".glsl") )
		{
			std::cerr << *it << " doesn't end with .glsl extension.\n";
			continue;
		}

		std::string shaderName = shader_name (*it);

		// Write, one line at a time to the output
		std::ifstream fs (it->c_str(), std::ios::in);
		if ( !fs )
		{
			std::cerr << *it << " could not be opened.\n";
			continue;
		}

		output << "static const char *fallbackShader_" << shaderName << " = \"";
		while ( std::getline (fs, line) )
		{
			if ( line.empty() )
			{
				continue;
			}

			output << escape_string (line) << "\\n\\\n";
		}
		output << "\";\n\n";
	}

	// Group the shaders into programs using their names. This relies on the list being sorted.
	std::string programName = program_name (glslFiles.front());
	std::string shaderName = shader_name (glslFiles.front());

	output << "static const gpuShader_t " << programName << "_shaders[] = {\n";
	output << "\t{" << shader_enum_type (shaderName) << ", \"glsl/" << shader_file (glslFiles.front()) << "\", fallbackShader_" << shaderName << "},\n";

	headerOutput << "extern const gpuShaderProgram_t " << programName << "_program;\n";

	int numShaders = 1;
	for ( string_list::const_iterator it = glslFiles.begin() + 1;
			it != glslFiles.end(); ++it )
	{
		std::string thisProgramName = program_name (*it);
		shaderName = shader_name (*it);

		if ( programName != thisProgramName )
		{
			output << "};\n";

			output << "const gpuShaderProgram_t " << programName << "_program = {";
			output << "\"" << programName << "\", " << numShaders << ", " << programName << "_shaders};\n";

			programName = thisProgramName;
			output << "static const gpuShader_t " << programName << "_shaders[] = {\n";
			output << "\t{" << shader_enum_type (shaderName) << ", \"glsl/" << shader_file (*it) << "\", fallbackShader_" << shaderName << "},\n";

			headerOutput << "extern const gpuShaderProgram_t " << programName << "_program;\n";

			numShaders = 1;

			continue;
		}

		output << "\t{" << shader_enum_type (shaderName) << ", \"glsl/" << shader_file (*it) << "\", fallbackShader_" << shaderName << "},\n";
		numShaders++;
	}

	output << "};\n";
	output << "const gpuShaderProgram_t " << programName << "_program = {";
	output << "\"" << programName << "\", " << numShaders << ", " << programName << "_shaders};\n";

	// Write the .cpp file
	{
		std::ofstream ofs (cppFile.c_str(), std::ios::out);
		if ( !ofs )
		{
			std::cerr << "Could not create file " << cppFile << '\n';
		}

		ofs << output.str();
	}

	// Write the .h file
	{
		std::ofstream ofs (headerFile.c_str(), std::ios::out);
		if ( !ofs )
		{
			std::cerr << "Could not create file " << headerFile << '\n';
		}

		ofs << headerOutput.str();
	}
}