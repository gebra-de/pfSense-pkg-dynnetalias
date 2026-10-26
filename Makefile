# Copyright 2026 gebra
# SPDX-License-Identifier: Apache-2.0

PORTNAME=	pfSense-pkg-dynnetalias
PORTVERSION=	1.0.0
CATEGORIES=	net
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	projects@gbrands.de
COMMENT=	Dynamic interface network aliases for pfSense
WWW=	https://github.com/gebra-de/pfSense-pkg-dynnetalias

LICENSE=	APACHE20

NO_BUILD=	yes
NO_MTREE=	yes

BUILD_ID?=	unknown
SOURCE_COMMIT?=	unknown

SUB_FILES=	pkg-install pkg-deinstall build-info
SUB_LIST=	PORTNAME=${PORTNAME} \
		PKGVERSION=${PKGVERSION} \
		BUILD_ID=${BUILD_ID} \
		SOURCE_COMMIT=${SOURCE_COMMIT}

do-extract:
	${MKDIR} ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/pkg ${STAGEDIR}${PREFIX}/libexec \
		${STAGEDIR}${DATADIR}
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/dynnetalias.xml \
		${STAGEDIR}${PREFIX}/pkg/dynnetalias.xml
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/dynnetalias.inc \
		${STAGEDIR}${PREFIX}/pkg/dynnetalias.inc
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/libexec/dynnetalias_worker.php \
		${STAGEDIR}${PREFIX}/libexec/dynnetalias_worker.php
	${INSTALL_DATA} ${FILESDIR}${DATADIR}/info.xml \
		${STAGEDIR}${DATADIR}/info.xml
	${INSTALL_DATA} ${WRKDIR}/build-info ${STAGEDIR}${DATADIR}/build-info
	@${REINPLACE_CMD} -i '' -e 's|%%PKGVERSION%%|${PKGVERSION}|g' \
		${STAGEDIR}${DATADIR}/info.xml ${STAGEDIR}${PREFIX}/pkg/dynnetalias.xml

.include <bsd.port.mk>
