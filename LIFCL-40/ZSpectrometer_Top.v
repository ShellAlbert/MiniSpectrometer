module ZSpectrometer_Top(
    input wire iExtClk,

    //MiniSpectrometer UART interaction IO.
    input wire iSpectrum_RxD, //T11.
    output wire oSpectrum_TxD, //T10.

    //upload UART to x86.
    output wire oUpload_TxD
);

//route spectrometer txd to upload txd directly.
//we parse data at x86 endian.
assign oUpload_TxD=iSpectrum_RxD;


ZProtocol myProtocol(
    .iClk(iExtClk),
    .iRstN(rst_n),

    input wire iEn,
    input wire iRxD,
    output wire oTxD
);

endmodule
