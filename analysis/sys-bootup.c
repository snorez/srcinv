/*
 * Init the system.
 *
 * For example, when the linux kernel bootup, it sets the hardware, generate
 * some variables, init them, etc. Also, check some sections in the image
 * file, call all the init functions.
 *
 * If we don't do sys_bootup(), we won't know what the variable is.
 *
 * Copyright (C) 2020 zerons
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

int sys_bootup(void)
{
	return 0;
}
