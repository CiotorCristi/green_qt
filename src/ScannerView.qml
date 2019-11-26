import QtMultimedia 5.13
import QtQuick 2.0
import QtQuick.Controls 2.5
import QZXing 2.3

VideoOutput {
    property Action cancelAction: Action {
        text: qsTr('id_cancel')
    }
    property var actions: [cancelAction]
    signal codeScanned(string code)

    autoOrientation: true
    fillMode: VideoOutput.PreserveAspectCrop
    filters: QZXingFilter {
        captureRect: {
            contentRect; sourceRect
            return mapRectToSource(mapNormalizedRectToItem(Qt.rect(0, 0, 1, 1)))
        }

        decoder {
            enabledDecoders: QZXing.DecoderFormat_QR_CODE
            onTagFound: codeScanned(tag)
        }
    }
}