//iXXX: input signal.
//oXXX: output signal.
//xxxN: negative signal.
`timescale 1ns/1ps

module ZUART_Rx
#(parameter Freq_divider=416)
(
	input wire iClk,
	input wire iRstN,
	input wire iEn,


	input wire iRxD,
	output reg oValid,
	output reg [7:0] oData
);

//generate 1MHz Clock. //48MHz/1MHz=48.
//generate 2MHz Clock. //48MHz/2M=24.
//generate 4MHz Clock, //48MHz/4MHz=12.
//generate 115200bps, 48MHz/115200bps=416.7

reg [15:0] cntBps;
always @(posedge iClk or negedge iRstN)
if(!iRstN) begin cntBps<=0; end
else begin 
	cntBps<=(iEn)?((cntBps==Freq_divider-1)?(0):(cntBps+1)):(0);
end

wire tickRx;
assign tickRx=(cntBps==Freq_divider-1)?(1):(0);

wire tickRxHalf;
assign tickRxHalf=(cntBps==Freq_divider/2)?(1):(0);

//Tx: start bit(1)+data bits(8)+stop bit(1)
//Tx Idle is High.
//Pull Low to start transfer, start bit is Low.
//8 bits data.
//1 Stop bit is High.
//RxD is 1 at idle, it is pulled down to 0 when start a new data.
//Falling edge detector.
reg [1:0] delayRxD;
always @(posedge iClk or negedge iRstN)
if(!iRstN) begin delayRxD<=2'b11; end
else begin
	delayRxD[1]<=(iEn)?(delayRxD[0]):(delayRxD[1]);
	delayRxD[0]<=(iEn)?(iRxD):(delayRxD[0]);
end

wire fallingRxD;
assign fallingRxD=iEn & delayRxD[1] & !delayRxD[0];

//driven by FSM.
reg [7:0] cntFSM;
reg [7:0] cntBits;
always @(posedge iClk or negedge iRstN)
if(!iRstN) begin
	cntFSM<=0; cntBits<=0; oValid<=0; oData<=0; 
end
else begin
	if(iEn) begin
		case(cntFSM)
			0: //falling edge detected.
				if(fallingRxD) begin oData<=0; cntFSM<=cntFSM+1; end
			1: //ignore start bit.
				if(tickRxHalf) begin cntFSM<=cntFSM+1; end
			2: //data bits(8).
				if(tickRxHalf) begin 
					oData[cntBits]<=iRxD; //LSB first.
					cntBits<=(cntBits==8-1)?(0):(cntBits+1);
					cntFSM<=(cntBits==8-1)?(cntFSM+1):(cntFSM);	
				end
			3: //stop bit(1).
				if(tickRxHalf) begin cntFSM<=cntFSM+1; end
			4:
				begin oValid<=1; cntFSM<=cntFSM+1; end
			5:
				begin oValid<=0; cntFSM<=cntFSM+1; end
			6:
				begin cntFSM<=0; end
			default:
				begin oValid<=0; oData<=0; cntFSM<=0; end
		endcase
	end
	else begin
			oValid<=0; oData<=0; cntFSM<=0; 
		end
end
endmodule