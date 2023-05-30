// process_files.cpp : Defines the entry point for the application.
//

#include "process_files.h"
#include "file-cpp/file.h"
#include "utf8-cpp/source/utf8.h"

using namespace std;

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <fstream>
#include <sstream>
#include <array>

#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

#include "fmt-9.1.0/include/fmt/format.h"

//<specifiers> <typename> <qualifiers>
std::string slurp(const char* filename)
{
	std::ifstream in;
	in.open(filename, std::ifstream::in | std::ifstream::binary);
	std::stringstream sstr;
	sstr << in.rdbuf();
	in.close();
	return sstr.str();
}

void slurp(const std::filesystem::path& filepath, std::string& buffer)
{
	std::ifstream in;
	buffer.clear();
	in.open(filepath, std::ifstream::in | std::ifstream::binary);
	if (in.is_open())
		buffer.assign((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
	in.close();
}

void slurp(const std::filesystem::path& filepath, std::vector<char>& buffer)
{
	std::ifstream in;
	in.open(filepath, std::ifstream::in | std::ifstream::binary);
	buffer.clear();
	buffer.assign((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
	in.close();
}

void string_from(std::string& buf, const std::filesystem::path& path)
{
	// buf.clear();

	if constexpr (std::is_same_v<std::filesystem::path::string_type, std::string>) {
		// buf.insert(buf.end(), path.native().data(), path.native().data() + path.native().size());
		// char_star(buf, path.native());

		buf.assign(path.native().data(), path.native().data() + path.native().size());
	}
	else if (std::is_same_v<std::filesystem::path::string_type, std::wstring>) {
		// buf = path.string();//char_star(buf, path.native());
#if _WIN32
		buf.clear();
		utf8::unchecked::utf16to8(
			path.native().data(), path.native().data() + path.native().size(), std::back_inserter(buf));
#else
		buf = path.string();
#endif
	}
	else if (std::is_same_v<std::filesystem::path::string_type, std::u8string>) {
		buf.assign(path.native().data(), path.native().data() + path.native().size());
	}
	else if (std::is_same_v<std::filesystem::path::string_type, std::u16string>) {
		buf.clear();
		utf8::unchecked::utf16to8(
			path.native().data(), path.native().data() + path.native().size(), std::back_inserter(buf));
	}
	else if (std::is_same_v<std::filesystem::path::string_type, std::u32string>) {
		buf.clear();
		utf8::unchecked::utf32to8(
			path.native().data(), path.native().data() + path.native().size(), std::back_inserter(buf));
	}
	else {
		buf = path.string();
	}
}

//c-string = char*
//std::string

//<x> <y> <w> <h> <label>
struct parse_result {
	std::string_view next = {};
	int ec = {};
};

struct label_data {
	int label;
	float x;
	float y;
	float w;
	float h;
	float conf;
};


//<int>
parse_result parse_int(int& inout, std::string_view line) {
	parse_result ret = {};
	size_t index = 0;
	for (; index < line.size(); index++) {
		char c = line[index];
		bool is_number = (c >= '0') and (c <= '9');
		bool is_symbol = c == '-' || c == '+';
		if (!(is_number || is_symbol))
			break;
	}
	std::from_chars_result result = std::from_chars(line.data(), line.data() + index, inout, 10);
	if ((int)result.ec != 0) {
		ret.ec = (decltype(ret.ec))result.ec;
		ret.next = line.substr(1);
		return ret;
	}

	size_t parsed = std::distance(line.data(), result.ptr);
	line = line.substr(parsed);
	size_t whitespace_idk = 0;
	for (; whitespace_idk < line.size(); whitespace_idk++) {
		if (!std::isspace(line[whitespace_idk])) {
			break;
		}
	}
	ret.next = line.substr(whitespace_idk);
	return ret;
}

parse_result parse_float(float& flt, std::string_view line) {
	parse_result ret = {};
	size_t index = 0;
	for (; index < line.size(); index++) {
		char c = line[index];
		bool is_number = (c >= '0') and (c <= '9');
		bool is_symbol = c == '.' || c == 'e' || c == 'E' || c == '+';
		if (!(is_number || is_symbol))
			break;
	}
	std::from_chars_result result = std::from_chars(line.data(), line.data() + index, flt);
	if ((int)result.ec != 0) {
		ret.ec = (decltype(ret.ec))result.ec;
		ret.next = line.substr(1);
		return ret;
	}

	size_t parsed = std::distance(line.data(), result.ptr);
	line = line.substr(parsed);
	size_t whitespace_idk = 0;
	for (; whitespace_idk < line.size(); whitespace_idk++) {
		if (!std::isspace(line[whitespace_idk])) {
			break;
		}
	}
	ret.next = line.substr(whitespace_idk);
	return ret;
}

parse_result parse_to_end_of_line(std::string& label, std::string_view line) {
	parse_result lbl = {};
	size_t index = 0;
	for (; index < line.size(); index++) {
		char c = line[index];
		bool is_newline = c == '\n' || c == '\r' || c == '\v' || c == '\f';
		if (is_newline) {
			break;
		}
	}
	label.assign(line.data(), index);
	line = line.substr(index);
	size_t whitespace_idk = 0;
	for (; whitespace_idk < line.size(); whitespace_idk++) {
		if (!std::isspace(line[whitespace_idk])) {
			break;
		}
	}
	lbl.next = line.substr(whitespace_idk);
	return lbl;
}

// <x> <y> <w> <h> <label>`
parse_result parse_line(label_data& label, std::string_view line) {
	parse_result ret = parse_int(label.label, line);
	if (ret.ec != 0) {
		return ret;
	}

	ret = parse_float(label.x, ret.next);
	if (ret.ec != 0) {
		return ret;
	}

	ret = parse_float(label.y, ret.next);
	if (ret.ec != 0) {
		return ret;
	}
	ret = parse_float(label.w, ret.next);
	if (ret.ec != 0) {
		return ret;
	}
	ret = parse_float(label.h, ret.next);
	if (ret.ec != 0) {
		return ret;
	}
	ret = parse_float(label.conf, ret.next);

	return ret;
}

parse_result parse_camera_name(std::string& camera_name, std::string_view folder_path) {
	parse_result ret = {};
	std::string_view parent_folder = util::utf8::parent_path(folder_path);
	std::string_view folder_name = folder_path.substr(parent_folder.size());

	folder_name = folder_name.substr(folder_name.find_first_not_of("\\/"));

	size_t i = 0;
	for (; i < folder_name.size(); i++) {
		char c = folder_name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		}
		else {
			break;
		}
	}
	if (i == 0) {
		ret.ec = 1;
		ret.next = folder_path.substr(1);
		return ret;
	}

	size_t size = i;
	for (; i < folder_name.size(); i++) {
		char c = folder_name[i];
		if ((c >= '0' && c <= '9')) {
		}
		else {
			break;
		}

	}

	if (i == size) {
		ret.ec = 2;
		ret.next = folder_path.substr(1);
		return ret;
	}

	camera_name.assign(folder_name.data(), i);
	const char *last = folder_name.data() + i;
	size_t removed = last - parent_folder.data();
	ret.next = folder_path.substr(removed);
	//ret.next = folder_path.substr(parent_folder.size()+i);

	return ret;
}

float width(label_data* self) {
	return self->w;
}

int main(int argc, char* argv[])
{
	fmt::print("{}", "hello!\n");

	if (argc < 1) {
		return 1;
	}

	std::cout << argv[0] << '\n';
	if (argc < 2) {
		std::cout << "<folder_path>\n";
		return 2;
	}

	std::vector<std::string_view> args;
	args.reserve(argc - 1);
	for (int i = 2; i < argc; i++)
		args.emplace_back(argv[i]);

	if (args.size() && args[0].compare("--sorted") == 0) {
		args.erase(args.begin());
		std::sort(args.begin(), args.end());
	}

	//const char *folder_path = "Z:\\Training photos\\SMC21_31_23";
	const char* const folder_path = argv[1];
	std::error_code ec{};
	std::filesystem::directory_iterator iterator = { folder_path, ec };

	std::array<std::string_view, 4> extensions = { ".jpg", ".tiff", ".png", ".jpeg" };

	std::filesystem::path p;
	p.stem();
	p.extension();
	label_data label;
	std::string tmp;
	std::string buf;
	std::string tmp_filepath;
	//std::string tmp_filebuffer;
	if (ec == std::error_code{}) {
		std::string_view folder_path_vw = folder_path;
		std::string_view parent_folder = util::utf8::parent_path(folder_path_vw);
		//foder/someting/thing
		//folder/something
		std::string_view folder_name = folder_path_vw.substr(parent_folder.size());
		folder_name = folder_name.substr(folder_name.find_first_not_of("\\/"));

		std::string csv_path;
		fmt::format_to(std::back_inserter(csv_path), "{}\\{}.csv", folder_path, folder_name);
		//fmt::print("<fmtstring>", ...);
		fmt::print("{}\n", csv_path);

		std::string tmp_filebuffer;
		tmp_filebuffer.reserve(1024);
		
		std::string camera_name;
		parse_result ret = parse_camera_name(camera_name, folder_path_vw);
		if (ret.ec) {
			return ret.ec;
		}

		std::string_view deployment_date = ret.next.substr(ret.next.find_first_not_of(" \t\v\f\r\n"));

		std::vector<size_t> counts;
		counts.reserve(256);
		std::vector<size_t> idxs;
		idxs.reserve(256);

		std::fstream out_filestream;
		out_filestream.open(csv_path, std::ios::trunc | std::ios::out | std::ios::binary);
		if (out_filestream.is_open()) {
			// example!
			tmp_filebuffer.clear();
			fmt::format_to(std::back_inserter(tmp_filebuffer), "{}\n", "image name,camera name,deployment end date, created date,date time,label (integer),label (text),count");
			out_filestream.write(tmp_filebuffer.data(), tmp_filebuffer.size());

			for (const std::filesystem::directory_entry& entry : iterator) {
				//std::cout << entry.path() << '\n';
				//std::string entry_string = entry.path().string();
				//std::cout << entry_string << '\n';
				string_from(tmp, entry.path());

				for (size_t i = 0; i < tmp.size(); i++)
				{
					tmp[i] = std::tolower(tmp[i]);//(tmp[i] < 0x7f) ? (tmp[i] | 32) : tmp[i];
				}

				std::cout << tmp << '\n';
				std::string_view ext = util::utf8::extension(tmp);
				std::string_view file_path = std::string_view(tmp).substr(0, tmp.size() - ext.size());

				tmp_filepath.clear();
				tmp_filepath.append(file_path.data(), file_path.size());
				tmp_filepath.append(std::string_view{ ".txt" });

				bool found_extension = false;
				for (size_t i = 0; i < extensions.size(); i++) {
					if (extensions[i] == ext) {
						found_extension = true;
						break;
					}
				}

				if (!found_extension) {
					continue;
				}

				struct stat filetime;
				bool s_ok = stat(file_path.data(), &filetime) == 0;

				if (!s_ok)
					continue;

				auto ctime = filetime.st_ctime;
				auto mtime = filetime.st_mtime;

				struct tm ctime_tm = {};
				localtime_s(&ctime_tm, &ctime);

				struct tm mtime_tm = {};
				localtime_s(&mtime_tm, &mtime);

				// slurp the text file
				slurp(tmp_filepath, buf);

				parse_result result = {};
				result.next = std::string_view{ buf.data(),buf.size() };

				counts.clear();
				idxs.clear();
				for (;;) {
					if (result.next.size() <= 0)
						break;
					result = parse_line(label, result.next);
					if (result.ec != 0)
						break;

					auto idx_it = std::find(idxs.begin(), idxs.end(), size_t{ (size_t)label.label });
					size_t idx = std::distance(idxs.begin(), idx_it);
					if (idx_it == idxs.end()) {
						idxs.emplace_back(size_t{(size_t)label.label});
						counts.emplace_back(0);
					}

					counts[idx]++;
				}
				
				bool has_counted = false;
				for (size_t ix = 0; ix < idxs.size(); ix++) {
					if (counts[ix]) {
						tmp_filebuffer.clear();
						fmt::format_to(std::back_inserter(tmp_filebuffer), "{},{},{},{}/{}/{} {}:{}:{},{}/{}/{} {}:{}:{},{},{},{}\n",
							tmp,
							camera_name,
							deployment_date,
							ctime_tm.tm_mon+1,
							ctime_tm.tm_mday,
							ctime_tm.tm_year + 1900,
							ctime_tm.tm_hour,
							ctime_tm.tm_min,
							ctime_tm.tm_sec,
							mtime_tm.tm_mon + 1,
							mtime_tm.tm_mday,
							mtime_tm.tm_year + 1900,
							mtime_tm.tm_hour,
							mtime_tm.tm_min,
							mtime_tm.tm_sec,
							idxs[ix],
							(idxs[ix] < args.size()) ? args[idxs[ix]] : std::string_view{ "unknown_label" },
							counts[ix]
						);
						out_filestream.write(tmp_filebuffer.data(), tmp_filebuffer.size());
						has_counted = true;
					}
				}

				if (!has_counted) {
					tmp_filebuffer.clear();
					fmt::format_to(std::back_inserter(tmp_filebuffer), "{},{},{},{}/{}/{} {}:{}:{},{}/{}/{} {}:{}:{},,,\n",
						tmp,
						camera_name,
						deployment_date,
						ctime_tm.tm_mon + 1,
						ctime_tm.tm_mday,
						ctime_tm.tm_year + 1900,
						ctime_tm.tm_hour,
						ctime_tm.tm_min,
						ctime_tm.tm_sec,
						mtime_tm.tm_mon + 1,
						mtime_tm.tm_mday,
						mtime_tm.tm_year + 1900,
						mtime_tm.tm_hour,
						mtime_tm.tm_min,
						mtime_tm.tm_sec
						//~size_t{ 0 },
						//(idxs[ix] < args.size()) ? args[idxs[ix]] : std::string_view{ "unknown_label" },
						//counts[ix]
					);
					out_filestream.write(tmp_filebuffer.data(), tmp_filebuffer.size());
				}
				
				//"image name,camera name,deployment date,date,time,label (integer),label (text),count"
				/*
				tmp_filebuffer.clear();
				fmt::format_to(std::back_inserter(tmp_filebuffer), "{},{},{},{},{},{},{},{}\n", tmp, 
					camera_name,
					);
				out_filestream.write(tmp_filebuffer.data(), tmp_filebuffer.size());
				*/

				
#if 0
					//
					bool found_extension = false;
				for (size_t i = 0; i < extensions.size(); i++) {
					if (extensions[i] == ext) {
						found_extension = true;
						break;
					}
				}

				if (found_extension) {
					std::fstream out_filestream;
					out_filestream.open(tmp_filepath, std::ios::trunc | std::ios::out | std::ios::binary);
					if (out_filestream.is_open()) {
						std::cout << ext << '\n';
						{ //.PNG .PnG 
							// process this file~
							slurp(entry.path(), buf);

							std::cout << buf.data() << '\n';
							tmp_filebuffer.clear();

							parse_result result = {};
							result.next = std::string_view{ buf.data(),buf.size() };
							for (;;) {
								if (result.next.size() <= 0)
									break;
								result = parse_line(label, result.next);
								if (result.ec != 0)
									break;

								//
								auto it = std::find_if(args.begin(), args.end(), [&label](const std::string_view vw) {
									return label.label.compare(vw) == 0;
									});
								if (it == args.end()) {
									continue;
								}

								size_t label_index = std::distance(args.begin(), it);
								tmp_filebuffer.reserve(tmp_filebuffer.size() + ((32 + 1) * 5));
								std::to_chars_result r = std::to_chars(tmp_filebuffer.data() + tmp_filebuffer.size(), tmp_filebuffer.data() + tmp_filebuffer.capacity(), label_index);
								if ((int)r.ec != 0) {
									break;
								}
								*r.ptr = ' ';
								tmp_filebuffer.assign(tmp_filebuffer.data(), r.ptr + 1);

								r = std::to_chars(tmp_filebuffer.data() + tmp_filebuffer.size(), tmp_filebuffer.data() + tmp_filebuffer.capacity(), label.x);
								if ((int)r.ec != 0) {
									break;
								}
								*r.ptr = ' ';
								tmp_filebuffer.assign(tmp_filebuffer.data(), r.ptr + 1);

								r = std::to_chars(tmp_filebuffer.data() + tmp_filebuffer.size(), tmp_filebuffer.data() + tmp_filebuffer.capacity(), label.y);
								if ((int)r.ec != 0) {
									break;
								}
								*r.ptr = ' ';
								tmp_filebuffer.assign(tmp_filebuffer.data(), r.ptr + 1);

								r = std::to_chars(tmp_filebuffer.data() + tmp_filebuffer.size(), tmp_filebuffer.data() + tmp_filebuffer.capacity(), label.w);
								if ((int)r.ec != 0) {
									break;
								}
								*r.ptr = ' ';
								tmp_filebuffer.assign(tmp_filebuffer.data(), r.ptr + 1);

								r = std::to_chars(tmp_filebuffer.data() + tmp_filebuffer.size(), tmp_filebuffer.data() + tmp_filebuffer.capacity(), label.h);
								if ((int)r.ec != 0) {
									break;
								}
								*r.ptr = '\n';
								tmp_filebuffer.assign(tmp_filebuffer.data(), r.ptr + 1);
							}
							out_filestream.write(tmp_filebuffer.data(), tmp_filebuffer.size());
							std::cout << tmp_filebuffer.data() << '\n';
							out_filestream.close();
						}
					}
				}
#endif
			}

			out_filestream.close();
		}
		else {
			fmt::print("could not open {}", csv_path);
		}
	}
	else {
		fmt::print("could not open folder {}", folder_path);
	}
	return 0;
}
