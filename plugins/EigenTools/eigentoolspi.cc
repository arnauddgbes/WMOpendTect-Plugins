/*
 *   EigenTools Plugin - miscellaneous Eigen based tools
 *  Copyright (C) 2021  Wayne Mogg
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "eigen_toolsmod.h"

mDefODPluginInfo(EigenTools)
{
    mDefineStaticLocalObject( PluginInfo, retpi,(
	"EigenTools plugin",
	wmPlugins::sKeyWMPlugins(),
	wmPlugins::sKeyWMPluginsAuthor(),
	wmPlugins::sKeyWMPluginsVersion(),
	"Various Eigen based tools used in other plugins.") );
    return &retpi;
}

mDefODInitPlugin(EigenTools)
{
     return 0;
}
