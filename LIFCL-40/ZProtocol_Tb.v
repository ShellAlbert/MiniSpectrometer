`timescale 1ns/1ps
module ZProtocol_Tb;

//dump FSDB for verdi.
initial begin
    $fsdbDumpfile("My.fsdb");
    $fsdbDumpvars(0, "ZProtocol_Tb.myProtocol");
end

reg clk;
initial begin 
    clk=0;
end
always #10 clk=~clk;

reg rst_n;
initial begin
    rst_n=0; 
    #100; 
    rst_n=1;
end

reg rxd;
initial begin rxd=0; end

wire txd;
ZProtocol myProtocol(
    .iClk(clk),
    .iRstN(rst_n),

    .iEn(1),
    .iRxD(rxd),
    .oTxD(txd)
);
endmodule