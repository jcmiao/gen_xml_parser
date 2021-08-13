#ifndef GCONF_GEN_H_
#define GCONF_GEN_H_

#include <list>
#include "desc.h"

bool GenerateXmlReaderCode(const gconf::Desc &desc, const std::list<std::string> &struct_names);

#endif /* GCONF_GEN_H_ */