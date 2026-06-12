module ZProtocol(
    input wire iClk,
    input wire iRstN,

    input wire iEn,
    input wire iRxD,
    output wire oTxD
);

//upload baudrate 1Mbps.
reg [7:0] txdata_reg;
reg tx_en;
wire tx_done;
ZUART_Tx #(.Freq_divider(416)) my_UART_Tx
(
	.iClk(iClk),
	.iRst_N(iRstN),
	.iData(txdata_reg),

	//pull down iEn to start transmition until pulse done oDone was issued.
	.iEn(tx_en),
	.oDone(tx_done),
	.oTxD(oTxD)
);

//uart protocol.
//reg [wordsize : 0] array_name [0 : arraysize];
reg [7:0] cmd_supported_range[8:0];
reg [7:0] cmd_get_frame[8:0];
initial begin
    //unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x0F,0xE5,0x0D,0x0A};
    cmd_supported_range[0]=8'hCC;
    cmd_supported_range[1]=8'h01;
    cmd_supported_range[2]=8'h09;
    cmd_supported_range[3]=8'h00;
    cmd_supported_range[4]=8'h00;
    cmd_supported_range[5]=8'h0F;
    cmd_supported_range[6]=8'hE5;
    cmd_supported_range[7]=8'h0D;
    cmd_supported_range[8]=8'h0A;

    //unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x02,0xD8,0x0D,0x0A};
    cmd_get_frame[0]=8'hCC;
    cmd_get_frame[1]=8'h01;
    cmd_get_frame[2]=8'h09;
    cmd_get_frame[3]=8'h00;
    cmd_get_frame[4]=8'h00;
    cmd_get_frame[5]=8'h02;
    cmd_get_frame[6]=8'hD8;
    cmd_get_frame[7]=8'h0D;
    cmd_get_frame[8]=8'h0A;
end
reg [7:0] step_i;
reg [7:0] array_index;
reg [15:0] cntDelay;
always @(posedge iClk or negedge iRstN)
if(!iRstN) begin
    step_i<=0; 
    tx_en<=0; 
    array_index<=0; 
    cntDelay<=0; 
end
else begin
    if(iEn) begin
        case(step_i)
        0: //get supported range.
            if(tx_done) begin tx_en<=0; step_i<=step_i+1; end
            else begin tx_en<=1; txdata_reg<=cmd_supported_range[array_index]; end
        1: //delay for a while.
            if(cntDelay==16'hFFF-1) begin cntDelay<=0; step_i<=step_i+1; end
            else begin cntDelay<=cntDelay+1; end
        2: 
            if(array_index==8) begin array_index<=0; step_i<=step_i+1; end
            else begin array_index<=array_index+1; step_i<=step_i-2; end
        3: //delay for a while.
            if(cntDelay==16'hFFFF-1) begin cntDelay<=0; step_i<=step_i+1; end
            else begin cntDelay<=cntDelay+1; end
        4: //get single frame.
            if(tx_done) begin tx_en<=0; step_i<=step_i+1; end
            else begin tx_en<=1; txdata_reg<=cmd_get_frame[array_index]; end
        5: 
            if(array_index==8) begin array_index<=0; step_i<=step_i+1; end
            else begin array_index<=array_index+1; step_i<=step_i-1; end
        6: //delay for a while.
            if(cntDelay==16'hFFFF-1) begin cntDelay<=0; step_i<=step_i+1; end
            else begin cntDelay<=cntDelay+1; end
        7: //repeat to query single frame.
            begin step_i<=step_i-3; end
        default:
            begin step_i<=0; end
        endcase
    end
    else begin
        step_i<=0; 
        tx_en<=0; 
        array_index<=0; 
        cntDelay<=0; 
    end
end
endmodule