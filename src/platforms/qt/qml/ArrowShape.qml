// SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import QtQuick.Shapes 1.6

Shape {
	height: 20
	width: 20
	ShapePath {
		id: arrow
		strokeWidth: 2
		strokeColor: "white"
		fillColor: "transparent"
		joinStyle: ShapePath.MiterJoin
		strokeStyle: ShapePath.SolidLine
		startX: 6; startY: 10
		PathLine { x: 10; y: 6 }
		PathLine { x: 10; y: 8 }
		PathLine { x: 14; y: 8 }
		PathLine { x: 14; y: 12 }
		PathLine { x: 10; y: 12 }
		PathLine { x: 10; y: 14 }
		PathLine { x: arrow.startX; y: arrow.startY }
	}
}
