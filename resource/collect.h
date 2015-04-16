/*
* Copyright (c) 2015 GuangZhou Research Institute of China Telecom . and others.  All rights reserved.
*
* This program and the accompanying materials are made available under the
* terms of the Eclipse Public License v1.0 which accompanies this distribution,
* and is available at http://www.eclipse.org/legal/epl-v10.html
*/
/**
* This model implements system config reading and init system params
* <p/>
*
* @author Peng li (chinatelecom.sdn.group@gmail.com)
* @version 0.1
*          <p/>
* @since 2015-03-23
*/

#include <stdlib.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>
xmlDocPtr get_ResourceInfo();