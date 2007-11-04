// RegHandler.h: interface for the RegHandler class.
//
//////////////////////////////////////////////////////////////////////

#ifndef REGHANDLER_H
#define REGHANDLER_H

#include "ConfigHandler.h"

#include <string>
#include "windows.h"
#include <winreg.h>

using std::string;

class RegHandler: public ConfigHandler 
{
public:
	RegHandler(string keyname,HKEY key=HKEY_CURRENT_USER);
	virtual ~RegHandler();
	virtual void SetInt(std::string name, int value);
	virtual void SetString(std::string name, std::string value);
	virtual std::string GetString(std::string name, std::string def);
	virtual int GetInt(std::string name, int def);
	virtual float GetFloat(const std::string& name, const float def);
protected:
	HKEY regkey;
};

#endif // REGHANDLER_H
