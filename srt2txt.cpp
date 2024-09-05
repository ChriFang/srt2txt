#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#endif


static const bool kPrintDbgLog = false;


#ifdef _WIN32
void listSrtFiles(std::vector<std::string> &list, const std::string& dir)
{
  intptr_t handle;
  struct _finddata_t fileinfo;

  std::string onePath = dir + "\\*.srt";
  handle = _findfirst(onePath.c_str(), &fileinfo);
  if (handle == -1) return;

  do
  {
    if (strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, ".."))
    {
      if (fileinfo.attrib & _A_ARCH || fileinfo.attrib & _A_NORMAL)
      {
        std::string srcFile = dir + "\\" + fileinfo.name;
        list.push_back(srcFile);
      }
    }
  } while (!_findnext(handle, &fileinfo));

  _findclose(handle);
}
#else
void listSrtFiles(std::vector<std::string> &list, const std::string& dir)
{
  list.clear();

  std::string path = dir;
  if (path.empty())
  {
    return;
  }

  DIR* dp;
  if ((dp = opendir(path.c_str())) == NULL) return;

  struct dirent* dirp;
  while ((dirp = readdir(dp)) != NULL)
  {
    std::string name = dirp->d_name;
    if (name == ".." || name == ".")
    {
      continue;
    }

    struct stat fileInfo;
    std::string fullFileName = path + "/" + name;
    if (stat(fullFileName.c_str(), &fileInfo) != 0)
    {
        continue;
    }

    if (strstr(name.c_str(), ".srt") != NULL)
    {
        list.push_back(fullFileName);
    }
  }

  closedir(dp);
}
#endif

// if eof, return false
bool readLine(FILE* fp, std::string& line, bool& isComplete)
{
    int c = fgetc(fp);
    while (c != EOF)
    {
        line.push_back((char)c);
        if ((char)c == '\n')
        {
            isComplete = true;
            return true;
        }
        c = fgetc(fp);
    }

    isComplete = (line.length() != 0);
    return isComplete;
}

bool storeResults(const std::string& fileName, const std::vector<std::string>& results)
{
    FILE* fp = fopen(fileName.c_str(), "w");
    if (fp == nullptr)
    {
        std::cout << "open file " << fileName << "for write failed" << std::endl;
        return false;
    }

    for (auto it : results)
    {
        std::string& line = it;
        size_t len = fwrite((void*)line.c_str(), 1, (size_t)line.length(), fp);
        if (len != line.length())
        {
            std::cout << "write file len " << len << ", line len" << line.length() << std::endl;
        }
    }

    fclose(fp);
    return true;
}

void trimCRLF(std::string& line)
{
    std::string tmp;
    for (int i = 0; i < line.length(); i++)
    {
        if (line[i] != '\r' && line[i] != '\n')
        {
            tmp += line[i];
        }
    }
    tmp += '\n';
    line = tmp;
}

static const char kExcludeCharList[] = {'?', '!'};
void trimHeadChar(std::string& line)
{
    bool isFind = false;
    int excludeListLen = sizeof(kExcludeCharList)/sizeof(char);
    for (int i = 0; i < excludeListLen; i++)
    {
        if (line[0] == kExcludeCharList[i])
        {
            isFind = true;
            break;
        }
    }
    if (isFind)
    {
        line = line.substr(1);
    }
}

bool trimOneHtmlTag(std::string& line)
{
    size_t pos1 = 0, pos2 = 0;
    bool hasLeftBracket = false;
    for (size_t i = 0; i < line.length(); i++)
    {
        if (line[i] == '<')
        {
            pos1 = i;
            hasLeftBracket = true;
        }
        else if (line[i] == '>' && hasLeftBracket)
        {
            pos2 = i;
            break;
        }
    }
    if (pos2 > pos1)
    {
        line.replace(pos1, pos2 - pos1 + 1, "");
        return true;
    }
    return false;
}

void trimHtmlTag(std::string& line)
{
    bool success = false;
    do {
        success = trimOneHtmlTag(line);
    } while (success);
}

std::string getTxtFileName(const std::string& srtFileName)
{
    std::string txtFileName = srtFileName;
    size_t pos = txtFileName.find(".srt");
    if (pos == std::string::npos)
    {
        txtFileName += ".txt";
    }
    else
    {
        txtFileName.replace(pos, 4, ".txt");
    }
    return txtFileName;
}

int converSrt2Txt(const std::string& srcFile, const std::string dstFile)
{
    FILE* fp = fopen(srcFile.c_str(), "r");
    if (fp == nullptr)
    {
        std::cout << "open file " << srcFile << "failed" << std::endl;
        return -1;
    }

    std::vector<std::string> lines;
    std::vector<std::string> results;
    while (1)
    {
        std::string line;
        bool isComplete = false;
        if (!readLine(fp, line, isComplete))
        {
            std::cout << "conversion " << srcFile << " end" << std::endl;
            break;
        }

        if (isComplete)
        {
            lines.push_back(line);
            if (line == "\n" || line == "\r\n")
            {
                if (lines.size() >= 3)
                {
                    /*
                     * the format of one unit is as below (https://en.wikipedia.org/wiki/SubRip)
                     * - 1. A numeric counter identifying each sequential subtitle
                     * - 2. The time that the subtitle should appear on the screen, followed by --> and the time it should disappear
                     * - 3. Subtitle text itself on one or more lines
                     * - 4. A blank line containing no text, indicating the end of this subtitle
                     * we only take the subtitle text here.
                    */
                   trimCRLF(lines[2]);
                   trimHtmlTag(lines[2]);
                   trimHeadChar(lines[2]);
                   results.push_back(lines[2]);
                }
                else if (lines.size() == 1)
                {
                    // only one empty line, strip it
                }
                else
                {
                    std::cout << "invalid SubRip file format" << std::endl;
                }
                lines.clear();
            }
        }
    }

    if (kPrintDbgLog)
    {
        std::vector<std::string>::iterator iter = results.begin();
        while (iter != results.end())
        {
            std::cout << *iter << std::endl;
            iter++;
        }
    }

    storeResults(dstFile, results);

    fclose(fp);
    return 0;
}

int main()
{
    // list all srt files from current directory
    std::vector<std::string> srtFileList;
    listSrtFiles(srtFileList, ".");

    for (auto it : srtFileList)
    {
        std::string txtFileName = getTxtFileName(it);
        converSrt2Txt(it, txtFileName);
    }

    return 0;
}