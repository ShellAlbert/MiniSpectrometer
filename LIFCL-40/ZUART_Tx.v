`timescale 1ns/1ps

module ZUART_Tx #(parameter Freq_divider=416)
(
	input wire iClk,
	input wire iRst_N,
	input wire [7:0] iData,

	//pull down iEn to start transmition until pulse done oDone was issued.
	input wire iEn,
	output reg oDone,
	output reg oTxD
);

//generate 1MHz Clock. //48MHz/1MHz=48.
//generate 2MHz Clock. //48MHz/2M=24.
//generate 4MHz Clock, //48MHz/4MHz=12.
//generate 115200bps, 48MHz/115200bps=416.7

//We expect a 100MHz PLL output, but the report gives out the maximum frequency is 51MHz.
//Single Clock Domain
//-------------------------------------------------------------------------------------------------------
//	Clock clk_100MHz            |                    |       Period       |     Frequency      
//-------------------------------------------------------------------------------------------------------
//	 From clk_100MHz                        |             Target |          10.000 ns |        100.002 MHz 
//											| Actual (all paths) |          19.442 ns |         51.435 MHz 
//100MHz/2MHz=5.
//51.435MHz/2MHz=25.7175
reg [15:0] cntBps;
always @(posedge iClk or negedge iRst_N)
if(!iRst_N) begin cntBps<=0; end
else begin 
	if(iEn) begin 
				if(cntBps==Freq_divider-1) begin cntBps<=0; end
				else begin cntBps<=cntBps+1; end
			end
	else begin 
		cntBps<=0;
	end
end

wire tickTx;
assign tickTx=(cntBps==Freq_divider-1)?(1):(0);

//Tx: start bit(1)+data bits(8)+stop bit(1)
//Tx Idle is High.
//Pull Low to start transfer, start bit is Low.
//8 bits data.
//1 Stop bit is High.
reg [7:0] cntFSM;
reg [7:0] cntBits;
always @(posedge iClk or negedge iRst_N)
if(!iRst_N) begin
	oTxD<=1; //Idle is High.
	oDone<=0; cntFSM<=0; cntBits<=0;
end
else begin
	if(iEn) begin
		case(cntFSM)
			0: //start bit(1).
				if(tickTx) begin oTxD<=0; cntFSM<=cntFSM+1; end
			1: //data bits(8).
				if(tickTx) begin 
					//oTxD<=iData[7-cntBits]; //MSB first.
					//oTxD<=iData[cntBits]; //LSB first.
					//oTxD<=~iData[cntBits]; //LSB first.
					oTxD<=iData[cntBits]; //LSB first.

					cntBits<=(cntBits==8-1)?(0):(cntBits+1);
					cntFSM<=(cntBits==8-1)?(cntFSM+1):(cntFSM);
					//issue done signal at previous clock.
					oDone<=(cntBits==8-1)?(1):(0);		
				end
			2: //stop bit(1).
				begin
					if(tickTx) begin oTxD<=1; cntFSM<=0; end
					/////////////////////////////////////////////////////
					oDone<=0; 
				end
			default:
				begin oTxD<=1; oDone<=0; cntFSM<=0; end
		endcase
	end
	else begin
			oTxD<=1; oDone<=0; cntFSM<=0; 
		end
end
endmodule