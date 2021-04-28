// SPDX-FileCopyrightText: 2021 Rolf Eike Beer <eike@sf-mail.de>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.0
import QtLocation 5.6
import QtPositioning 5.6
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Shapes 1.6

Rectangle {
	property variant initialArea: QtPositioning.rectangle()
	property variant selectedArea: initialArea
	property int imperialUnits: 0

	TabBar {
		id: tabbar
		objectName: "tabbar"
		anchors {
			top: parent.top
			left: parent.left
			right: parent.right
		}

		TabButton {
			text: qsTr("Map")
		}
		TabButton {
			text: qsTr("Direct")
		}
		TabButton {
			text: qsTr("Extent")
		}
	}

	StackLayout {
		anchors {
			top: tabbar.bottom
			left: parent.left
			right: parent.right
			bottom: parent.bottom
		}
		currentIndex: tabbar.currentIndex

		Map {
			id: map
			plugin: Plugin {
				name: "osm"
			}
			gesture.enabled: true
			gesture.acceptedGestures: shSelect.active ? MapGestureArea.PinchGesture : (MapGestureArea.PinchGesture | MapGestureArea.PanGesture)
			visibleRegion : initialArea

			Repeater {
				model: otherBounds
				MapRectangle {
					border {
						color: modelData == initialArea ? "blue" : "darkGrey"
						width: 2
					}
					topLeft: modelData.topLeft
					bottomRight: modelData.bottomRight
				}
			}

			// the current selection
			MapRectangle {
				border {
					color: "green"
					width: 2
				}
				z: 1
				topLeft: selectedArea.topLeft
				bottomRight: selectedArea.bottomRight
				visible: selectedArea != initialArea
			}

			// The overlay for select or move
			Rectangle {
				width: 40
				height: 2 * width
				color: "black"
				opacity: 0.5
				radius: width / 5
				layer.enabled: true
				anchors {
					right: parent.right
					verticalCenter: parent.verticalCenter
				}
				// the "selection" shape
				Shape {
					id: shSelect
					property bool active: false
					opacity: active ? 1.0 : 0.5
					readonly property int icon_border: width / 5
					readonly property int icon_size: width - 2 * icon_border
					readonly property int icon_line_w: width / 20

					anchors {
						top: parent.top
						left: parent.left
						right: parent.right
						bottom: parent.verticalCenter
					}
					ShapePath {
						id: pathSolid
						readonly property int max_coord: shSelect.icon_size
						strokeWidth: 2
						capStyle: ShapePath.FlatCap
						fillColor: "transparent"
						strokeColor: "white"
						strokeStyle: ShapePath.SolidLine
						startX: shSelect.icon_border; startY: shSelect.icon_border
						PathLine { x: pathSolid.max_coord; y: pathSolid.startY }
						PathLine { x: pathSolid.max_coord; y: pathSolid.max_coord }
						PathLine { x: pathSolid.startX; y: pathSolid.max_coord }
						PathLine { x: pathSolid.startX; y: pathSolid.startY }
					}
					ShapePath {
						id: pathDashed
						readonly property int max_coord: shSelect.icon_size + shSelect.icon_border
						strokeWidth: 2
						capStyle: ShapePath.FlatCap
						fillColor: "transparent"
						strokeColor: "white"
						strokeStyle: ShapePath.DashLine
						dashPattern: [ shSelect.icon_line_w / 2, shSelect.icon_line_w / 2 ]
						startX: pathSolid.max_coord; startY: shSelect.icon_border
						// start and end positions adjusted to not overlap the previous rectangle
						PathLine { x: pathDashed.max_coord; y: pathDashed.startY }
						PathLine { x: pathDashed.max_coord; y: pathDashed.max_coord }
						PathLine { x: pathSolid.startX; y: pathDashed.max_coord }
						PathLine { x: pathSolid.startX; y: pathSolid.max_coord }
					}
					MouseArea {
						anchors.fill: parent
						onClicked: shSelect.active = true
					}
				}
				// the "move" arrows
				Item {
					property bool active: !shSelect.active
					property real shapeOpacity: active ? 1.0 : 0.5
					anchors {
						top: shSelect.bottom
						bottom: parent.bottom
						left: parent.left
						right: parent.right
						margins: 0
					}

					// left arrow
					ArrowShape {
						anchors {
							left: parent.left
							verticalCenter: parent.verticalCenter
						}
						opacity: parent.shapeOpacity
					}
					// up arrow
					ArrowShape {
						anchors {
							top: parent.top
							horizontalCenter: parent.horizontalCenter
						}
						opacity: parent.shapeOpacity
						rotation: 90
					}
					// right arrow
					ArrowShape {
						anchors {
							right: parent.right
							verticalCenter: parent.verticalCenter
						}
						opacity: parent.shapeOpacity
						rotation: 180
					}
					// down arrow
					ArrowShape {
						anchors {
							bottom: parent.bottom
							horizontalCenter: parent.horizontalCenter
						}
						opacity: parent.shapeOpacity
						rotation: 270
					}
					MouseArea {
						anchors.fill: parent
						onClicked: shSelect.active = false
					}
				}
			}
		}
		Rectangle {
			id: direct
			// direct editing of bounds

			GridLayout {
				id: lyDirect
				anchors {
					left: parent.left
					right: parent.right
					top: parent.top
					margins: 5
				}
				columns: 4
				Layout.margins: 5

				DegreeInput {
					id: minLat
					objectName: "minLat"
					degrees: selectedArea.bottomRight.latitude
					latitude: true
					placeholderText: qsTr("minimum latitude in degrees")
					onEditingFinished: selectedArea.bottomRight = QtPositioning.coordinate(degrees, maxLon.degrees)
				}
				Text {
					text: qsTr("° to")
				}
				DegreeInput {
					id: maxLat
					objectName: "maxLat"
					degrees: selectedArea.topLeft.latitude
					latitude: true
					placeholderText: qsTr("maximum latitude in degrees")
					onEditingFinished: selectedArea.topLeft = QtPositioning.coordinate(degrees, minLon.degrees)
				}
				Text {
					text: qsTr("°")
				}

				DegreeInput {
					id: minLon
					objectName: "minLon"
					degrees: selectedArea.topLeft.longitude
					latitude: false
					placeholderText: qsTr("minimum longitude in degrees")
					onEditingFinished: selectedArea.topLeft = QtPositioning.coordinate(maxLat.degrees, degrees)
				}
				Text {
					text: qsTr("° to")
				}
				DegreeInput {
					id: maxLon
					objectName: "maxLon"
					degrees: selectedArea.bottomRight.longitude
					latitude: false
					placeholderText: qsTr("maximum longitude in degrees")
					onEditingFinished: selectedArea.bottomRight = QtPositioning.coordinate(minLat.degrees, degrees)
				}
				Text {
					text: qsTr("°")
				}
			}
			Text {
				anchors {
					top: lyDirect.bottom
					left: parent.left
					right: parent.right
					margins: 5
				}
				horizontalAlignment: Text.AlignHCenter

				text: qsTr("(recommended min/max diff <0.03 degrees)")
			}
		}
		Rectangle {
			id: extent
			// edit of center and dimension

			GridLayout {
				id: lyExtent
				anchors {
					left: parent.left
					right: parent.right
					top: parent.top
					margins: 5
				}
				columns: 3
				Layout.margins: 5

				Text {
					text: qsTr("Center:")
				}
				DegreeInput {
					id: centerLat
					objectName: "centerLat"
					degrees: selectedArea.center.latitude
					latitude: true
					placeholderText: qsTr("center latitude in degrees")
					onEditingFinished: {
						// one cannot just update the center, so the edges have to be moved
						var delta = degrees - selectedArea.center.latitude
						selectedArea.topLeft = QtPositioning.coordinate(selectedArea.topLeft.latitude + delta, selectedArea.topLeft.longitude)
						selectedArea.bottomRight = QtPositioning.coordinate(selectedArea.bottomRight.latitude + delta, selectedArea.bottomRight.longitude)
					}
				}
				DegreeInput {
					id: centerLon
					objectName: "centerLon"
					degrees: selectedArea.center.longitude
					latitude: false
					placeholderText: qsTr("center longitude in degrees")
					onEditingFinished: {
						// one cannot just update the center, so the edges have to be moved
						var delta = degrees - selectedArea.center.longitude
						selectedArea.topLeft = QtPositioning.coordinate(selectedArea.topLeft.latitude, selectedArea.topLeft.longitude + delta)
						selectedArea.bottomRight = QtPositioning.coordinate(selectedArea.bottomRight.latitude, selectedArea.bottomRight.longitude + delta)
					}
				}

				Text {
					text: qsTr("Width:")
				}
				TextField {
					id: wdth
					objectName: "extentWidth"
					text: Number(selectedArea.topLeft.distanceTo(QtPositioning.coordinate(selectedArea.topLeft.latitude, selectedArea.bottomRight.longitude)) / 1000 /
							unit.model.get(unit.currentIndex).divider).toLocaleString(Qt.locale(), 'f', 5)
					validator: DoubleValidator {
						bottom: 0
						top: 20
						notation: DoubleValidator.StandardNotation
					}
					onEditingFinished: {
						var meterOffset = Number.fromLocaleString(Qt.locale(), text) * 1000 / 2
						var oldCenter = selectedArea.center
						selectedArea.topLeft = QtPositioning.coordinate(selectedArea.topLeft.latitude, oldCenter.atDistanceAndAzimuth(meterOffset, 270).longitude)
						selectedArea.bottomRight = QtPositioning.coordinate(selectedArea.bottomRight.latitude, oldCenter.atDistanceAndAzimuth(meterOffset, 90).longitude)
					}
					color: acceptableInput ? "black" : "red"
				}
				ComboBox {
					id: unit
					Layout.rowSpan: 2
					textRole: "unit"
					model: ListModel {
						ListElement { unit: qsTr("mi"); divider: 1.609344 }
						ListElement { unit: qsTr("km"); divider: 1 }
					}
					currentIndex: imperialUnits ? 0 : 1
					onActivated: imperialUnits = 1 - currentIndex
				}

				Text {
					text: qsTr("Height:")
				}
				TextField {
					id: hght
					objectName: "extentHeight"
					text: Number(selectedArea.topLeft.distanceTo(QtPositioning.coordinate(selectedArea.bottomRight.latitude, selectedArea.topLeft.longitude)) / 1000 /
							unit.model.get(unit.currentIndex).divider).toLocaleString(Qt.locale(), 'f', 5)
					validator: DoubleValidator {
						bottom: 0
						top: 20
						notation: DoubleValidator.StandardNotation
					}
					onEditingFinished: {
						var meterOffset = Number.fromLocaleString(Qt.locale(), text) * 1000 / 2
						var oldCenter = selectedArea.center
						selectedArea.topLeft = QtPositioning.coordinate(oldCenter.atDistanceAndAzimuth(meterOffset, 0).latitude, selectedArea.topLeft.longitude)
						selectedArea.bottomRight = QtPositioning.coordinate(oldCenter.atDistanceAndAzimuth(meterOffset, 180).latitude, selectedArea.bottomRight.longitude)
					}
					color: acceptableInput ? "black" : "red"
				}
			}
			Text {
				anchors {
					top: lyExtent.bottom
					left: parent.left
					right: parent.right
					margins: 5
				}
				horizontalAlignment: Text.AlignHCenter

				text: unit.currentIndex == 0 ? qsTr("(recommended width/height < 1.25mi)") : qsTr("(recommended width/height < 2km)")
			}
		}
	}
}
