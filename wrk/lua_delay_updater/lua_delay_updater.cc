#include <sstream>
#include <fstream>
#include <string>

int
main(int argc, char* argv[])
{
	if (argc != 3) {
		printf("Need 2 parameters, only %d found.\n", argc-1);
		exit(0);
	}

	int pos;
	std::ifstream fp(argv[1]);
	std::stringstream ss;
	std::string content, tmp;

	content = "dl_time = ";
	content += argv[2];
	content += "\n";
 
	ss << fp.rdbuf();
	tmp = ss.str();
	
	pos = tmp.find("\n");
	tmp = tmp.substr(pos, tmp.length() - pos);
	pos = tmp.find_first_not_of("\n ");
	tmp = tmp.substr(pos, tmp.length() - pos);

	tmp = content + tmp;
	fp.close();

	std::ofstream ofp(argv[1]);

	ofp.write(tmp.c_str(), tmp.length());
	
	ofp.close();
	return 0;
}

