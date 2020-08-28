/* QEMU will only user data portion, all header information is specified in the command line */
DefinitionBlock (
    "ssdt.aml",
    "SSDT",
    0x00000001,
    "INTEL",
    "INTLSSDT",
    0x00000001
    )
{
    External (_SB_.PCI0, DeviceObj)
    Scope (_SB.PCI0) {
        Device (GPI0) {
            Name (LINK, "\\_SB.PCI0.GPI0")
            /* Match VirtoIO GPIO PCI in guest */
            Name (_ADR, 0x00140000)
            Name (_DDN, "VirtoIO GPIO Controller")
            Name (_UID, "VirtoIO GPIO Controller 0")
            /* Empty _CRS, VirtoIO GPIO FE will handle all PV resource */
            Method (_CRS, 0, Serialized) {
                Return (Buffer(Zero){})
            }
            Method (_STA, 0, NotSerialized) {
                Return (0x0F)
            }
        }

        Device (I2C1) {
            Name (_ADR, 0x00150001)
            Device (NFC1) {
                Name (_ADR, Zero)  // _ADR: Address
                Name (_HID, EisaId ("NXP1002"))  // _HID: Hardware ID
                Name (_CID, "NXP1002")  // _CID: Compatible ID
                Name (_DDN, "NXP NFC")  // _DDN: DOS Device Name
                Name (_UID, One)  // _UID: Unique ID
                Method (_STA, 0, NotSerialized) {
                    Return (0x0F)
                }
                Method (_CRS, 0, Serialized) {
                    Name (SBUF, ResourceTemplate () {
                        I2cSerialBusV2 (0x0029, ControllerInitiated, 0x00061A80,
                            AddressingMode7Bit, "\\_SB.PCI0.I2C1",
                            0x00, ResourceConsumer, _Y55, Exclusive,
                            )
                        GpioInt (Edge, ActiveHigh, ExclusiveAndWake, PullDefault, 0x0000,
                            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
                            )
                            {
                                0x0001
                            }
                        GpioIo (Exclusive, PullDefault, 0x0000, 0x0000, IoRestrictionOutputOnly,
                            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
                            )
                            {
                                0x0002
                            }
                        GpioIo (Exclusive, PullDefault, 0x0000, 0x0000, IoRestrictionOutputOnly,
                            "\\_SB.PCI0.GPI0", 0x00, ResourceConsumer, ,
                            )
                            {
                                0x0003
                            }
                    })
                    CreateWordField (SBUF, \_SB.PCI0.I2C1.NFC1._CRS._Y55._ADR, NADR)
                    CreateDWordField (SBUF, \_SB.PCI0.I2C1.NFC1._CRS._Y55._SPE, NSPD)
                    //CreateWordField (SBUF, 0x38, NFCA)
                    //CreateWordField (SBUF, 0x60, NFCB)
                    //CreateWordField (SBUF, 0x88, NFCC)
                    NADR = 0x29
                    NSPD = 0x00061A80
                    //NFCA = 0x0000
                    //NFCB = 0x0001
                    //NFCC = 0x0002
                    Return (SBUF)
                }
            }
        }
    }
}
